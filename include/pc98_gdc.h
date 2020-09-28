
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

    void begin_frame(void);
    void next_line(void);

    void load_display_partition(void);
    void next_display_partition(void);

    size_t fifo_can_read(void);
    bool fifo_empty(void);
    uint16_t read_fifo(void);

    /* NTS:
     *
     * We're following the Neko Project II method of FIFO emulation BUT
     * I wonder if the GDC maintains two FIFOs and allows stacking params
     * in one and commands in another....? */

    uint8_t                 cmd_parm_tmp[8];            /* temp storage before accepting params */

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
};

typedef union pc98_tile             egc_quad[4];

extern bool                         gdc_analog;

extern uint32_t                     pc98_text_palette[8];

extern struct PC98_GDC_state        pc98_gdc[2];
extern egc_quad                     pc98_gdc_tiles;
extern uint8_t                      pc98_gdc_vramop;
extern uint8_t                      pc98_gdc_modereg;

