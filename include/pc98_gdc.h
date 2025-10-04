
#include "dos_inc.h"

#define PC98_GDC_FIFO_SIZE      32      /* taken from Neko Project II, but what is it really? */
#define GDC_COMMAND_BYTE        0x100

enum {
    GDC_MASTER=0,
    GDC_SLAVE=1
};

// VOPBIT_* source: Neko Project II

// operate:		bit0	access page
//				bit1	egc enable
//				bit2	grcg bit6
//				bit3	grcg bit7
//				bit4	analog enable (16/256-color mode). 8-color mode if not.
//				bit5	pc9821 vga

enum {
	VOPBIT_ACCESS	= 0,
	VOPBIT_EGC		= 1,
	VOPBIT_GRCG		= 2,
	VOPBIT_ANALOG	= 4,
	VOPBIT_VGA		= 5,
    VOPBIT_PEGC_PLANAR = 6
};

union pc98_tile {
    uint8_t                 b[2];
    uint16_t                w;
};

struct PC98_GDC_state {
    PC98_GDC_state();
    void reset_fifo(void);
    void reset_rfifo(void);
    void flush_fifo_old(void);
    bool write_fifo(const uint16_t c);
    bool write_rfifo(const uint8_t c);
    bool write_fifo_command(const unsigned char c);
    bool write_fifo_param(const unsigned char c);
    bool rfifo_has_content(void);
    uint8_t read_status(void);
    uint8_t rfifo_read_data(void);
    void idle_proc(void);

    void force_fifo_complete(void);
    void take_cursor_char_setup(unsigned char bi);
    void take_cursor_pos(unsigned char bi);
    void take_reset_sync_parameters(void);
    void cursor_advance(void);

    void draw_reset(void);
    void vectw(unsigned char bi);
    void set_vectw(uint8_t ope, uint8_t dir, uint16_t dc, uint16_t d, uint16_t d2, uint16_t d1, uint16_t dm);
    void exec(uint8_t command);
    void prepare(void);
    void draw_dot(uint16_t x, uint16_t y);
    void pset(void);
    void line(void);
    void text(void);
    void circle(void);
    void box(void);
    void set_vectl(int x1, int y1, int x2, int y2);
    void set_mode(uint8_t mode);
    void set_csrw(uint32_t ead, uint8_t dad);
    void set_textw(uint16_t pattern);
    void set_textw(uint8_t *tile, uint8_t len);

    void begin_frame(void);
    void next_line(void);

    void load_display_partition(void);
    void next_display_partition(void);

    size_t fifo_can_read(void);
    bool fifo_empty(void);
    uint16_t read_fifo(void);

    enum {
        RT_MULBIT   = 15,
        RT_TABLEBIT = 12,
        RT_TABLEMAX = (1 << RT_TABLEBIT)
    };
    struct VECTDIR {
        int16_t x;
        int16_t y;
        int16_t x2;
        int16_t y2;
    };
    static const VECTDIR vectdir[16];
    static const PhysPt gram_base[4];
    static uint16_t gdc_rt[RT_TABLEMAX + 1];

    struct GDC_DRAW {
        PhysPt base;
        uint32_t ead;
        uint16_t dc;
        uint16_t d;
        uint16_t d1;
        uint16_t d2;
        uint16_t dm;
        uint16_t pattern;
        uint16_t x;
        uint16_t y;
        uint8_t tx[8];
        uint8_t dad;
        uint8_t ope;
        uint8_t dir;
        uint8_t dgd;
        uint8_t wg;
        uint8_t dots;
        uint8_t mode;
        uint8_t zoom;
    } draw;

    /* NTS:
     *
     * We're following the Neko Project II method of FIFO emulation BUT
     * I wonder if the GDC maintains two FIFOs and allows stacking params
     * in one and commands in another....? */

    uint8_t                 cmd_parm_tmp[11];            /* temp storage before accepting params */

    uint8_t                 rfifo[PC98_GDC_FIFO_SIZE];
    uint8_t                 rfifo_read,rfifo_write;

    uint16_t                fifo[PC98_GDC_FIFO_SIZE];   /* NTS: Neko Project II uses one big FIFO for command and data, which makes sense to me */
    uint8_t                 fifo_read,fifo_write;

    uint8_t                 param_ram[16];
    uint8_t                 param_ram_wptr;

    uint16_t                scan_address;
    uint8_t                 row_height;
    uint8_t                 row_line;

    uint8_t                 display_partition;
    uint16_t                display_partition_rem_lines;
    uint8_t                 display_partition_mask;

    uint16_t                active_display_lines;       /* AL (translated) */
    uint16_t                active_display_words_per_line;/* AW bits (translated) */
    uint16_t                display_pitch;
    uint8_t                 horizontal_sync_width;      /* HS (translated) */
    uint8_t                 vertical_sync_width;        /* VS (translated) */
    uint8_t                 horizontal_front_porch_width;/* HFP (translated) */
    uint8_t                 horizontal_back_porch_width;/* HBP (translated) */
    uint8_t                 vertical_front_porch_width; /* VFP (translated) */
    uint8_t                 vertical_back_porch_width;  /* VBP (translated) */
    uint8_t                 display_mode;               /* CG bits */
            /* CG = 00 = mixed graphics & character
             * CG = 01 = graphics mode
             * CG = 10 = character mode
             * CG = 11 = invalid */
    uint8_t                 video_framing;              /* IS bits */
            /* IS = 00 = non-interlaced
             * IS = 01 = invalid
             * IS = 10 = interlaced repeat field for character displays
             * IS = 11 = interlaced */
    uint8_t                 current_command;
    uint8_t                 proc_step;
    uint8_t                 cursor_blink_state;
    uint8_t                 cursor_blink_count;         /* count from 0 to BR - 1 */
    uint8_t                 cursor_blink_rate;          /* BR */
    bool                    draw_only_during_retrace;   /* F bits */
    bool                    dynamic_ram_refresh;        /* D bits */
    bool                    master_sync;                /* master source generation */
    bool                    display_enable;
    bool                    cursor_enable;
    bool                    cursor_blink;
    bool                    IM_bit;                     /* display partition, IM bit */
    bool                    idle;

    bool                    doublescan;                 /* 200-line as 400-line */

    bool                    dbg_ev_partition;

    double                  drawing_end;
    int                     dot_count;
    uint8_t                 drawing_status;
};

typedef union pc98_tile             egc_quad[4];

extern bool                         gdc_analog;

extern uint32_t                     pc98_text_palette[8];

extern struct PC98_GDC_state        pc98_gdc[2];
extern egc_quad                     pc98_gdc_tiles;
extern uint8_t                      pc98_gdc_vramop;
extern uint8_t                      pc98_gdc_modereg;

extern uint8_t                      pc98_gdc_tile_counter;
extern uint8_t                      pc98_gdc_modereg;
extern uint8_t                      pc98_gdc_vramop;
extern egc_quad                     pc98_gdc_tiles;

extern uint8_t                      pc98_egc_srcmask[2]; /* host given (Neko: egc.srcmask) */
extern uint8_t                      pc98_egc_maskef[2]; /* effective (Neko: egc.mask2) */
extern uint8_t                      pc98_egc_mask[2]; /* host given (Neko: egc.mask) */

extern bool                         pc98_timestamp5c;

extern uint8_t                      GDC_display_plane_wait_for_vsync;

Bitu pc98_read_9a8(Bitu /*port*/,Bitu /*iolen*/);
void pc98_write_9a8(Bitu port,Bitu val,Bitu iolen);

void gdc_5mhz_mode_update_vars(void);
void pc98_port6A_command_write(unsigned char b);
void pc98_wait_write(Bitu port,Bitu val,Bitu iolen);
void pc98_crtc_write(Bitu port,Bitu val,Bitu iolen);
void pc98_port68_command_write(unsigned char b);
Bitu pc98_read_9a0(Bitu /*port*/,Bitu /*iolen*/);
void pc98_write_9a0(Bitu port,Bitu val,Bitu iolen);
Bitu pc98_crtc_read(Bitu port,Bitu iolen);
Bitu pc98_a1_read(Bitu port,Bitu iolen);
void pc98_a1_write(Bitu port,Bitu val,Bitu iolen);
void pc98_gdc_write(Bitu port,Bitu val,Bitu iolen);
Bitu pc98_gdc_read(Bitu port,Bitu iolen);
Bitu pc98_egc4a0_read(Bitu port,Bitu iolen);
void pc98_egc4a0_write(Bitu port,Bitu val,Bitu iolen);
Bitu pc98_egc4a0_read_warning(Bitu port,Bitu iolen);
void pc98_egc4a0_write_warning(Bitu port,Bitu val,Bitu iolen);

extern const UINT8 gdc_defsyncm15[8];
extern const UINT8 gdc_defsyncs15[8];

extern const UINT8 gdc_defsyncm24[8];
extern const UINT8 gdc_defsyncs24[8];

extern const UINT8 gdc_defsyncm31[8];
extern const UINT8 gdc_defsyncs31[8];

extern const UINT8 gdc_defsyncm31_480[8];
extern const UINT8 gdc_defsyncs31_480[8];

void PC98_Set24KHz(void);
void PC98_Set31KHz(void);
void PC98_Set31KHz_480line(void);

