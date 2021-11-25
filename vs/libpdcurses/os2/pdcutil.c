/* Public Domain Curses */

#include "pdcos2.h"

#if defined(OS2) && !defined(__EMX__)
APIRET APIENTRY DosSleep(ULONG ulTime);
#endif

void PDC_beep(void)
{
    PDC_LOG(("PDC_beep() - called\n"));

    DosBeep(1380, 100);
}

ULONG PDC_ms_count(void)
{
    ULONG now;

    DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, &now, sizeof(ULONG));

    return now;
}

void PDC_napms(int ms)
{
    PDC_LOG(("PDC_napms() - called: ms=%d\n", ms));

    if ((SP->termattrs & A_BLINK) && (PDC_ms_count() >= pdc_last_blink + 500))
        PDC_blink_text();

    DosSleep(ms);
}

const char *PDC_sysname(void)
{
    return "OS/2";
}
