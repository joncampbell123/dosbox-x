/* Public Domain Curses */

#include "pdcsdl.h"

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
    int ret_vis;

    PDC_LOG(("PDC_curs_set() - called: visibility=%d\n", visibility));

    ret_vis = SP->visibility;

    SP->visibility = visibility;

    PDC_gotoyx(SP->cursrow, SP->curscol);

    return ret_vis;
}

void PDC_set_title(const char *title)
{
    PDC_LOG(("PDC_set_title() - called:<%s>\n", title));

    SDL_WM_SetCaption(title, title);
}

int PDC_set_blink(bool blinkon)
{
    if (pdc_color_started)
        COLORS = 256;

    if (blinkon)
    {
        if (!(SP->termattrs & A_BLINK))
        {
            SP->termattrs |= A_BLINK;
            PDC_blink_text();
        }
    }
    else
    {
        if (SP->termattrs & A_BLINK)
        {
            SP->termattrs &= ~A_BLINK;
            PDC_blink_text();
        }
    }

    return OK;
}

int PDC_set_bold(bool boldon)
{
#ifdef PDC_WIDE
    if (boldon)
        SP->termattrs |= A_BOLD;
    else
        SP->termattrs &= ~A_BOLD;

    return OK;
#else
    return boldon ? ERR : OK;
#endif
}
