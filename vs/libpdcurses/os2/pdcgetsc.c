/* Public Domain Curses */

#include "pdcos2.h"

/* return width of screen/viewport */

int PDC_get_columns(void)
{
    VIOMODEINFO modeInfo = {0};
    int cols = 0;
    const char *env_cols;

    PDC_LOG(("PDC_get_columns() - called\n"));

    modeInfo.cb = sizeof(modeInfo);
    VioGetMode(&modeInfo, 0);
    cols = modeInfo.col;

    env_cols = getenv("COLS");
    if (env_cols)
        cols = min(atoi(env_cols), cols);

    PDC_LOG(("PDC_get_columns() - returned: cols %d\n", cols));

    return cols;
}

/* get the cursor size/shape */

int PDC_get_cursor_mode(void)
{
    VIOCURSORINFO cursorInfo;

    PDC_LOG(("PDC_get_cursor_mode() - called\n"));

    VioGetCurType (&cursorInfo, 0);

    return (cursorInfo.yStart << 8) | cursorInfo.cEnd;
}

/* return number of screen rows */

int PDC_get_rows(void)
{
    VIOMODEINFO modeInfo = {0};
    int rows = 0;
    const char *env_rows;

    PDC_LOG(("PDC_get_rows() - called\n"));

    /* use the value from LINES environment variable, if set. MH 10-Jun-92 */
    /* and use the minimum of LINES and *ROWS.                MH 18-Jun-92 */

    modeInfo.cb = sizeof(modeInfo);
    VioGetMode(&modeInfo, 0);
    rows = modeInfo.row;

    env_rows = getenv("LINES");
    if (env_rows)
        rows = min(atoi(env_rows), rows);

    PDC_LOG(("PDC_get_rows() - returned: rows %d\n", rows));

    return rows;
}
