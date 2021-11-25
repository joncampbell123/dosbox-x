/* Public Domain Curses */

#include "pdcx11.h"

#include <string.h>

/*man-start**************************************************************

pdcsetsc
--------

### Synopsis

    int PDC_set_blink(bool blinkon);
    int PDC_set_bold(bool boldon);
    void PDC_set_title(const char *title);

### Description

   PDC_set_blink() toggles whether the A_BLINK attribute sets an
   actual blink mode (TRUE), or sets the background color to high
   intensity (FALSE). The default is platform-dependent (FALSE in
   most cases). It returns OK if it could set the state to match
   the given parameter, ERR otherwise.

   PDC_set_bold() toggles whether the A_BOLD attribute selects an actual
   bold font (TRUE), or sets the foreground color to high intensity
   (FALSE). It returns OK if it could set the state to match the given
   parameter, ERR otherwise.

   PDC_set_title() sets the title of the window in which the curses
   program is running. This function may not do anything on some
   platforms.

### Portability
                             X/Open    BSD    SYS V
    PDC_set_blink               -       -       -
    PDC_set_title               -       -       -

**man-end****************************************************************/

int PDC_curs_set(int visibility)
{
    int ret_vis = SP->visibility;

    PDC_LOG(("PDC_curs_set() - called: visibility=%d\n", visibility));

    if (visibility != -1)
        SP->visibility = visibility;

    PDC_display_cursor(SP->cursrow, SP->curscol, SP->cursrow,
                       SP->curscol, visibility);

    return ret_vis;
}

void PDC_set_title(const char *title)
{
    int len;

    PDC_LOG(("PDC_set_title() - called:<%s>\n", title));

    len = strlen(title) + 1;        /* write nul character */

    XCursesInstruct(CURSES_TITLE);

    if (XC_write_display_socket_int(len) >= 0)
        if (XC_write_socket(xc_display_sock, title, len) >= 0)
            return;

    XCursesExitCursesProcess(1, "exiting from PDC_set_title");
}

int PDC_set_blink(bool blinkon)
{
    if (pdc_color_started)
        COLORS = 256;

    XCursesInstruct(blinkon ? CURSES_BLINK_ON : CURSES_BLINK_OFF);

    return OK;
}

int PDC_set_bold(bool boldon)
{
    if (boldon)
        SP->termattrs |= A_BOLD;
    else
        SP->termattrs &= ~A_BOLD;

    return OK;
}
