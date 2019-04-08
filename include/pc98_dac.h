
void pc98_update_palette(void);
void pc98_update_digpal(unsigned char ent);
void pc98_set_digpal_entry(unsigned char ent,unsigned char grb);
void pc98_set_digpal_pair(unsigned char start,unsigned char pair);
unsigned char pc98_get_digpal_pair(unsigned char start);
void VGA_DAC_UpdateColor( Bitu index );

extern uint32_t                 pc98_text_palette[8];
extern uint8_t                  pc98_16col_analog_rgb_palette_index;

extern uint8_t                  pc98_pal_vga[256*3];    /* G R B    0x0..0xFF */
extern uint8_t                  pc98_pal_analog[256*3]; /* G R B    0x0..0xF */
extern uint8_t                  pc98_pal_digital[8];    /* G R B    0x0..0x7 */

/* 4-bit to 6-bit expansion */
static inline unsigned char dac_4to6(unsigned char c4) {
    /* a b c d . .
     *
     * becomes
     *
     * a b c d a b */
    return (c4 << 2) | (c4 >> 2);
}

