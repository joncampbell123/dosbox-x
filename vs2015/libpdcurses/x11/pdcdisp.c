/* Public Domain Curses */

#include "pdcx11.h"

#include <string.h>

#ifdef CHTYPE_LONG

# define A(x) ((chtype)x | A_ALTCHARSET)

chtype acs_map[128] =
{
    A(0), A(1), A(2), A(3), A(4), A(5), A(6), A(7), A(8), A(9), A(10),
    A(11), A(12), A(13), A(14), A(15), A(16), A(17), A(18), A(19),
    A(20), A(21), A(22), A(23), A(24), A(25), A(26), A(27), A(28),
    A(29), A(30), A(31), ' ', '!', '"', '#', '$', '%', '&', '\'', '(',
    ')', '*',

# ifdef PDC_WIDE
    0x2192, 0x2190, 0x2191, 0x2193,
# else
    '>', '<', '^', 'v',
# endif

    '/',

# ifdef PDC_WIDE
    0x2588,
# else
    A(0),
# endif

    '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=',
    '>', '?', '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', '[', '\\', ']', '^', '_',

# ifdef PDC_WIDE
    0x2666, 0x2592,
# else
    A(1), A(2),
# endif

    'b', 'c', 'd', 'e',

# ifdef PDC_WIDE
    0x00b0, 0x00b1, 0x2591, 0x00a4, 0x2518, 0x2510, 0x250c, 0x2514,
    0x253c, 0x23ba, 0x23bb, 0x2500, 0x23bc, 0x23bd, 0x251c, 0x2524,
    0x2534, 0x252c, 0x2502, 0x2264, 0x2265, 0x03c0, 0x2260, 0x00a3,
    0x00b7,
# else
    A(7), A(8), '#', 0xa4, A(11), A(12), A(13), A(14), A(15), A(16),
    A(17), A(18), A(19), A(20), A(21), A(22), A(23), A(24), A(25),
    A(26), A(27), A(28), A(29), A(30), 0xb7,
# endif

    A(127)
};

# undef A

#endif

int PDC_display_cursor(int oldrow, int oldcol, int newrow, int newcol,
                       int visibility)
{
    char buf[30];
    int idx, pos;

    PDC_LOG(("%s:PDC_display_cursor() - called: NEW row %d col %d, vis %d\n",
             XCLOGMSG, newrow, newcol, visibility));

    if (visibility == -1)
    {
        /* Only send the CURSES_DISPLAY_CURSOR message, no data */

        idx = CURSES_DISPLAY_CURSOR;
        memcpy(buf, &idx, sizeof(int));
        idx = sizeof(int);
    }
    else
    {
        if ((oldrow == newrow) && (oldcol == newcol))
        {
            /* Do not send a message because it will cause the blink state
               to reset. */
            return OK;
        }

        idx = CURSES_CURSOR;
        memcpy(buf, &idx, sizeof(int));

        idx = sizeof(int);
        pos = oldrow + (oldcol << 8);
        memcpy(buf + idx, &pos, sizeof(int));

        idx += sizeof(int);
        pos = newrow + (newcol << 8);
        memcpy(buf + idx, &pos, sizeof(int));

        idx += sizeof(int);
    }

    if (XC_write_socket(xc_display_sock, buf, idx) < 0)
        XCursesExitCursesProcess(1, "exiting from PDC_display_cursor");

    return OK;
}

/* position hardware cursor at (y, x) */

void PDC_gotoyx(int row, int col)
{
    PDC_LOG(("PDC_gotoyx() - called: row %d col %d\n", row, col));

    PDC_display_cursor(SP->cursrow, SP->curscol, row, col, SP->visibility);
}

/* update the given physical line to look like the corresponding line in
   curscr */

void PDC_transform_line(int lineno, int x, int len, const chtype *srcp)
{
    PDC_LOG(("PDC_transform_line() - called: line %d\n", lineno));

    XC_get_line_lock(lineno);

    memcpy(Xcurscr + XCURSCR_Y_OFF(lineno) + (x * sizeof(chtype)), srcp,
           len * sizeof(chtype));

    *(Xcurscr + XCURSCR_START_OFF + lineno) = x;
    *(Xcurscr + XCURSCR_LENGTH_OFF + lineno) = len;

    XC_release_line_lock(lineno);

    XCursesInstructAndWait(CURSES_REFRESH);
}
