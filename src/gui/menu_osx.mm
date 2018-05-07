/* Mac OS X portion of menu.cpp */

#include "config.h"
#include "menu.h"

#if DOSBOXMENU_TYPE == DOSBOXMENU_NSMENU /* Mac OS X NSMenu / NSMenuItem handle */
# include <MacTypes.h>
# include <Cocoa/Cocoa.h>
# include <Foundation/NSString.h>

void* sdl_hax_nsMenuAlloc(const char *initWithText) {
	NSString *title = [[NSString alloc] initWithUTF8String:initWithText];
	NSMenu *menu = [[NSMenu alloc] initWithTitle: title];
	[title release];
	return (void*)menu;
}

void sdl_hax_nsMenuRelease(void *nsMenu) {
	[((NSMenu*)nsMenu) release];
}
#endif

