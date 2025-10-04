/* Public Domain Curses */

#include "pdcdos.h"

#include <stdlib.h>

/* return width of screen/viewport */

int PDC_get_columns(void)
{
    PDCREGS regs;
    int cols;
    const char *env_cols;

    PDC_LOG(("PDC_get_columns() - called\n"));

    /* use the value from COLS environment variable, if set. MH 10-Jun-92 */
    /* and use the minimum of COLS and return from int10h    MH 18-Jun-92 */

    regs.h.ah = 0x0f;
    PDCINT(0x10, regs);
    cols = (int)regs.h.ah;

    env_cols = getenv("COLS");

    if (env_cols)
        cols = min(atoi(env_cols), cols);

    PDC_LOG(("PDC_get_columns() - returned: cols %d\n", cols));

    return cols;
}

/* get the cursor size/shape */

int PDC_get_cursor_mode(void)
{
    PDC_LOG(("PDC_get_cursor_mode() - called\n"));

    return getdosmemword(0x460);
}

/* return number of screen rows */

int PDC_get_rows(void)
{
    const char *env_rows;
    int rows;

    PDC_LOG(("PDC_get_rows() - called\n"));

    /* use the value from LINES environment variable, if set. MH 10-Jun-92 */
    /* and use the minimum of LINES and *ROWS.                MH 18-Jun-92 */

    rows = getdosmembyte(0x484) + 1;
    env_rows = getenv("LINES");

    if (env_rows)
        rows = min(atoi(env_rows), rows);

    if (rows == 1 && pdc_adapter == _MDS_GENIUS)
        rows = 66;
    if (rows == 1 && pdc_adapter == _MDA)
        rows = 25;

    if (rows == 1)
    {
        rows = 25;
        pdc_direct_video = FALSE;
    }

    switch (pdc_adapter)
    {
    case _EGACOLOR:
    case _EGAMONO:
        switch (rows)
        {
        case 25:
        case 43:
            break;
        default:
            rows = 25;
        }
        break;

    case _VGACOLOR:
    case _VGAMONO:
        break;

    default:
        rows = 25;
        break;
    }

    PDC_LOG(("PDC_get_rows() - returned: rows %d\n", rows));

    return rows;
}
