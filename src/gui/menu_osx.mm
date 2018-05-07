/* Mac OS X portion of menu.cpp */

#include "config.h"
#include "menu.h"

#if DOSBOXMENU_TYPE == DOSBOXMENU_NSMENU /* Mac OS X NSMenu / NSMenuItem handle */
void* sdl_hax_nsMenuAlloc(const char *initWithText) {
	return NULL;
}

void sdl_hax_nsMenuRelease(void *nsMenu) {
}
#endif

