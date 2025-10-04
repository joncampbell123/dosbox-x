/* Public Domain Curses */

#include "pdcsdl.h"

/* get the cursor size/shape */

int PDC_get_cursor_mode(void)
{
    PDC_LOG(("PDC_get_cursor_mode() - called\n"));

    return 0;
}

/* return number of screen rows */

int PDC_get_rows(void)
{
    PDC_LOG(("PDC_get_rows() - called\n"));

    return pdc_sheight / pdc_fheight;
}

/* return width of screen/viewport */

int PDC_get_columns(void)
{
    PDC_LOG(("PDC_get_columns() - called\n"));

    return pdc_swidth / pdc_fwidth;
}
