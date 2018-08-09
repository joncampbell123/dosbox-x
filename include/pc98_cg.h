
#include "vga.h" /* uses VGA font RAM as CG ROM */

/* mapping function from 16-bit WORD to font RAM offset */
/* in: code = 16-bit JIS code word (unshifted)
 *     line = scan line within character cell
 *     right_half = 1 if right half, 0 if left half (double-wide only)
 * out: byte offset in FONT RAM (0x00000-0x7FFFF inclusive)
 *
 * NTS: Font ROM/RAM in PC-98 is laid out as follows in this emulation:
 *
 *      0x00 0xAA    ASCII single-wide character AA          offset = (AA * 16)
 *      0xHH 0xLL    Double-wide char (HH != 0) number HHLL  offset = ((HH & 0x7F) * 256) + ((LL & 0x7F) * 2) + right_half
 *
 *      Visual layout:
 *
 *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+................
 *      | | | | | | | | | | | | | | | | |................   0x00 to 0xFF, 8-bit wide, at 0x0000
 *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+................
 *      |   |   |   |   |   |   |   |   |................   0x0100 to 0x7F7F, 16-bit wide, at 0x0100, lower 7 bits only each byte
 *      +---+---+---+---+---+---+---+---+................
 *
 *      This is not necessarily how the font data is stored in ROM on actual hardware.
 *      The hardware appears to accept 16 bits but only use the low 7 bits of each byte for double-wide.
 *      0x80 0xAA is NOT an alias of single-wide chars. */
static inline uint32_t pc98_font_char_to_ofs(const uint16_t code,const uint8_t line,const uint8_t right_half) {
    if (code & 0xFF00) {
        /* double-wide. this maps 0x01-0x7F, 0x80 to 0x80, 0x81-0xFF to 0x01-0x7F */
        const uint16_t x_code = (code & 0x7F) + ((((code + 0x7F00) & 0x7F00) + 0x0100) >> 1); /* 16-bit to 14-bit conversion. */
        return ((((uint32_t)x_code * (uint32_t)16) + (uint32_t)(line & 0xF)) * (uint32_t)2) + (uint32_t)right_half;
    }
    else {
        /* single-wide */
        return ((uint32_t)code * (uint32_t)16) + (line & 0xF);
    }
}

static inline uint8_t pc98_font_char_read(const uint16_t code,const uint8_t line,const uint8_t right_half) {
    return vga.draw.font[pc98_font_char_to_ofs(code,line,right_half)];
}

static inline void pc98_font_char_write(const uint16_t code,const uint8_t line,const uint8_t right_half,const uint8_t byte) {
    vga.draw.font[pc98_font_char_to_ofs(code,line,right_half)] = byte;
}

