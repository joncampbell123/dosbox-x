/* Public Domain Curses */

#include "pdcx11.h"

/* return width of screen/viewport */

int PDC_get_columns(void)
{
    PDC_LOG(("PDC_get_columns() - called\n"));

    return XCursesCOLS;
}

/* get the cursor size/shape */

int PDC_get_cursor_mode(void)
{
    return 0;
}

/* return number of screen rows */

int PDC_get_rows(void)
{
    PDC_LOG(("PDC_get_rows() - called\n"));

    return XCursesLINES;
}
