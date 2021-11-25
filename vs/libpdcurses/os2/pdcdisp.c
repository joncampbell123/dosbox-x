/* Public Domain Curses */

#include "pdcos2.h"

/* ACS definitions originally by jshumate@wrdis01.robins.af.mil -- these
   match code page 437 and compatible pages (CP850, CP852, etc.) */

#ifdef CHTYPE_LONG

# define A(x) ((chtype)x | A_ALTCHARSET)

chtype acs_map[128] =
{
    A(0), A(1), A(2), A(3), A(4), A(5), A(6), A(7), A(8), A(9),
    A(10), A(11), A(12), A(13), A(14), A(15), A(16), A(17), A(18),
    A(19), A(20), A(21), A(22), A(23), A(24), A(25), A(26), A(27),
    A(28), A(29), A(30), A(31), ' ', '!', '"', '#', '$', '%', '&',
    '\'', '(', ')', '*',

    A(0x1a), A(0x1b), A(0x18), A(0x19),

    '/',

    0xdb,

    '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=',
    '>', '?', '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', '[', '\\', ']', '^', '_',

    A(0x04), 0xb1,

    'b', 'c', 'd', 'e',

    0xf8, 0xf1, 0xb0, A(0x0f), 0xd9, 0xbf, 0xda, 0xc0, 0xc5, 0x2d,
    0x2d, 0xc4, 0x2d, 0x5f, 0xc3, 0xb4, 0xc1, 0xc2, 0xb3, 0xf3,
    0xf2, 0xe3, 0xd8, 0x9c, 0xf9,

    A(127)
};

# undef A

#endif

ULONG pdc_last_blink;
static bool blinked_off = FALSE;

/* position hardware cursor at (y, x) */

void PDC_gotoyx(int row, int col)
{
    PDC_LOG(("PDC_gotoyx() - called: row %d col %d\n", row, col));

    VioSetCurPos(row, col, 0);
}

void _new_packet(attr_t attr, int lineno, int x, int len, const chtype *srcp)
{
    /* this should be enough for the maximum width of a screen. */

    char temp_line[256];
    int j;
    short fore, back;
    unsigned char mapped_attr;
    bool blink;

    PDC_LOG(("PDC_transform_line() - called: line %d\n", lineno));

    PDC_pair_content(PAIR_NUMBER(attr), &fore, &back);
    blink = (SP->termattrs & A_BLINK) && (attr & A_BLINK);

    if (blink)
        attr &= ~A_BLINK;

    if (attr & A_BOLD)
        fore |= 8;
    if (attr & A_BLINK)
        back |= 8;

    fore = pdc_curstoreal[fore];
    back = pdc_curstoreal[back];

    if (attr & A_REVERSE)
        mapped_attr = back | (fore << 4);
    else
        mapped_attr = fore | (back << 4);

    /* replace the attribute part of the chtype with the
       actual color value for each chtype in the line */

    for (j = 0; j < len; j++)
    {
        chtype ch = srcp[j];

#ifdef CHTYPE_LONG
        if (ch & A_ALTCHARSET && !(ch & 0xff80))
            ch = acs_map[ch & 0x7f];
#endif
        if (blink && blinked_off)
            ch = ' ';

        temp_line[j] = ch & 0xff;
    }

    VioWrtCharStrAtt((PCH)temp_line, (USHORT)len, (USHORT)lineno,
                     (USHORT)x, (PBYTE)&mapped_attr, 0);
}

/* update the given physical line to look like the corresponding line in
   curscr */

void PDC_transform_line(int lineno, int x, int len, const chtype *srcp)
{
    attr_t old_attr, attr;
    int i, j;

    PDC_LOG(("PDC_transform_line() - called: lineno=%d\n", lineno));

    old_attr = *srcp & (A_ATTRIBUTES ^ A_ALTCHARSET);

    for (i = 1, j = 1; j < len; i++, j++)
    {
        attr = srcp[i] & (A_ATTRIBUTES ^ A_ALTCHARSET);

        if (attr != old_attr)
        {
            _new_packet(old_attr, lineno, x, i, srcp);
            old_attr = attr;
            srcp += i;
            x += i;
            i = 0;
        }
    }

    _new_packet(old_attr, lineno, x, i, srcp);
}

void PDC_blink_text(void)
{
    int i, j, k;

    if (!(SP->termattrs & A_BLINK))
        blinked_off = FALSE;
    else
        blinked_off = !blinked_off;

    for (i = 0; i < SP->lines; i++)
    {
        const chtype *srcp = curscr->_y[i];

        for (j = 0; j < SP->cols; j++)
            if (srcp[j] & A_BLINK)
            {
                k = j;
                while (k < SP->cols && (srcp[k] & A_BLINK))
                    k++;
                PDC_transform_line(i, j, k - j, srcp + j);
                j = k;
            }
    }

    PDC_gotoyx(SP->cursrow, SP->curscol);
    pdc_last_blink = PDC_ms_count();
}
