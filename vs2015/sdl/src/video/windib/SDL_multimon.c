
#include "SDL_config.h"

/* THIS is why this function exists HERE, not in dibvideo.c.
 * VS2017 will not define HMONITOR or the required functions without WINVER >= 0x500 */
#undef WINVER
#define WINVER 0x500

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void SDL_windib_multimonitor_adjust_from_window(int *x, int *y, HWND hwnd) {
#if !defined(SDL_WIN32_HX_DOS)
    // Win98+ multi-monitor awareness.
    // Try to go fullscreen on whatever monitor this window exists on
    HMONITOR mon = NULL;

    mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (mon == NULL) mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);

    if (mon != NULL) {
        MONITORINFO mi;

        memset(&mi,0,sizeof(mi));
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfo(mon,&mi)) {
            *x += mi.rcMonitor.left;
            *y += mi.rcMonitor.top;
        }
    }
#endif
}

