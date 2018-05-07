/* Mac OS X portion of menu.cpp */

#include "config.h"
#include "menu.h"

#if DOSBOXMENU_TYPE == DOSBOXMENU_NSMENU /* Mac OS X NSMenu / NSMenuItem handle */
# include <MacTypes.h>
# include <Cocoa/Cocoa.h>
# include <Foundation/NSString.h>

extern "C" void* sdl1_hax_stock_osx_menu(void);

void* sdl_hax_nsMenuAlloc(const char *initWithText) {
	NSString *title = [[NSString alloc] initWithUTF8String:initWithText];
	NSMenu *menu = [[NSMenu alloc] initWithTitle: title];
	[title release];
	return (void*)menu;
}

void sdl_hax_nsMenuRelease(void *nsMenu) {
	[((NSMenu*)nsMenu) release];
}

void sdl_hax_macosx_setmenu(void *nsMenu) {
	if (nsMenu != NULL) {
		/* I like how Apple's shiny developer page on NSApplication mentions mainMenu
		   like you can assign to it but never mentions setMainMenu.

		   Come on Apple fix your documentation. List the headers you need to include.
		   List ALL the methods. Even Microsoft figured this out years ago >:( */
		[NSApp setMainMenu:((NSMenu*)nsMenu)];
	}
	else {
		/* switch back to the menu SDL 1.x made */
		[NSApp setMainMenu:((NSMenu*)sdl1_hax_stock_osx_menu())];
	}
}

void sdl_hax_nsMenuItemSetTag(void *nsMenuItem, unsigned int new_id) {
	[((NSMenuItem*)nsMenuItem) setTag:new_id];
}

void sdl_hax_nsMenuItemSetSubmenu(void *nsMenuItem,void *nsMenu) {
	[((NSMenu*)nsMenuItem) setSubmenu:((NSMenu*)nsMenu)];
}

void* sdl_hax_nsMenuItemAlloc(const char *initWithText) {
	NSString *title = [[NSString alloc] initWithUTF8String:initWithText];
	NSMenuItem *item = [[NSMenuItem alloc] initWithTitle: title action:nil keyEquivalent:@""];
	[title release];
	return (void*)item;
}

void sdl_hax_nsMenuAddItem(void *nsMenu,void *nsMenuItem) {
	[((NSMenu*)nsMenu) addItem:((NSMenuItem*)nsMenuItem)];
}

void* sdl_hax_nsMenuAllocSeparator(void) {
	return (void*)([NSMenuItem separatorItem]);
}

void sdl_hax_nsMenuItemRelease(void *nsMenuItem) {
	[((NSMenuItem*)nsMenuItem) release];
}
#endif

