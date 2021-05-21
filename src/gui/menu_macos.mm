/* Mac OS X portion of menu.cpp */

#include "config.h"
#include "menu.h"

#include "sdlmain.h"
#include "SDL.h"
#include "SDL_version.h"
#include "SDL_syswm.h"

#if DOSBOXMENU_TYPE == DOSBOXMENU_NSMENU /* Mac OS X NSMenu / NSMenuItem handle */
# include <MacTypes.h>
# include <Cocoa/Cocoa.h>
# include <Foundation/NSString.h>
# include <ApplicationServices/ApplicationServices.h>
# include <IOKit/pwr_mgt/IOPMLib.h>
# include <Cocoa/Cocoa.h>

@interface NSApplication (DOSBoxX)
@end

#if !defined(C_SDL2)
extern "C" void* sdl1_hax_stock_macosx_menu(void);
extern "C" void sdl1_hax_stock_macosx_menu_additem(NSMenu *modme);
extern "C" NSWindow *sdl1_hax_get_window(void);
#endif

char tempstr[4096];
bool InitCodePage(), CodePageGuestToHostUTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/);

void GetClipboard(std::string* result) {
	NSPasteboard* pb = [NSPasteboard generalPasteboard];
	NSString* text = [pb stringForType:NSPasteboardTypeString];
	*result = std::string([text UTF8String]);
}

bool SetClipboard(std::string value) {
	NSPasteboard* pb = [NSPasteboard generalPasteboard];
	NSString* text = [NSString stringWithUTF8String:value.c_str()];
	[pb clearContents];
	return [pb setString:text forType:NSStringPboardType];
}

void *sdl_hax_nsMenuItemFromTag(void *nsMenu, unsigned int tag) {
	NSMenuItem *ns_item = [((NSMenu*)nsMenu) itemWithTag: tag];
	return (ns_item != nil) ? ns_item : NULL;
}

void sdl_hax_nsMenuItemUpdateFromItem(void *nsMenuItem, DOSBoxMenu::item &item) {
	if (item.has_changed()) {
		NSMenuItem *ns_item = (NSMenuItem*)nsMenuItem;

		[ns_item setEnabled:(item.is_enabled() ? YES : NO)];
		[ns_item setState:(item.is_checked() ? NSOnState : NSOffState)];

		const std::string &it = item.get_text();
		const std::string &st = item.get_shortcut_text();
		std::string ft = it;

		/* TODO: Figure out how to put the shortcut text right-aligned while leaving the main text left-aligned */
		if (!st.empty()) {
			ft += " [";
			ft += st;
			ft += "]";
		}

		{
			NSString *title;
            int cp = dos.loaded_codepage;
            if (!dos.loaded_codepage) InitCodePage();
            if (CodePageGuestToHostUTF8(tempstr,ft.c_str()))
                title = [[NSString alloc] initWithUTF8String:tempstr];
            else
                title = [[NSString alloc] initWithString:[NSString stringWithFormat:@"%s",ft.c_str()]];
            dos.loaded_codepage = cp;
			[ns_item setTitle:title];
			[title release];
		}

		item.clear_changed();
	}
}

void* sdl_hax_nsMenuAlloc(const char *initWithText) {
	NSString *title;
    int cp = dos.loaded_codepage;
    if (!dos.loaded_codepage) InitCodePage();
    if (CodePageGuestToHostUTF8(tempstr,initWithText))
        title = [[NSString alloc] initWithUTF8String:tempstr];
    else
        title = [[NSString alloc] initWithString:[NSString stringWithFormat:@"%s",initWithText]];
    dos.loaded_codepage = cp;
	NSMenu *menu = [[NSMenu alloc] initWithTitle: title];
	[title release];
	[menu setAutoenablesItems:NO];
	return (void*)menu;
}

void sdl_hax_nsMenuRelease(void *nsMenu) {
	[((NSMenu*)nsMenu) release];
}

void sdl_hax_macosx_setmenu(void *nsMenu) {
	if (nsMenu != NULL) {
        /* switch to the menu object given */
		[NSApp setMainMenu:((NSMenu*)nsMenu)];
	}
	else {
#if !defined(C_SDL2)
		/* switch back to the menu SDL 1.x made */
		[NSApp setMainMenu:((NSMenu*)sdl1_hax_stock_macosx_menu())];
#endif
	}
}

void sdl_hax_nsMenuItemSetTag(void *nsMenuItem, unsigned int new_id) {
	[((NSMenuItem*)nsMenuItem) setTag:new_id];
}

void sdl_hax_nsMenuItemSetSubmenu(void *nsMenuItem,void *nsMenu) {
	[((NSMenuItem*)nsMenuItem) setSubmenu:((NSMenu*)nsMenu)];
}

void* sdl_hax_nsMenuItemAlloc(const char *initWithText) {
	NSString *title;
    int cp = dos.loaded_codepage;
    if (!dos.loaded_codepage) InitCodePage();
    if (CodePageGuestToHostUTF8(tempstr,initWithText))
        title = [[NSString alloc] initWithUTF8String:tempstr];
    else
        title = [[NSString alloc] initWithString:[NSString stringWithFormat:@"%s",initWithText]];
    dos.loaded_codepage = cp;
	NSMenuItem *item = [[NSMenuItem alloc] initWithTitle: title action:@selector(DOSBoxXMenuAction:) keyEquivalent:@""];
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

void sdl_hax_nsMenuAddApplicationMenu(void *nsMenu) {
#if defined(C_SDL2)
	/* make up an Application menu and stick it in first.
	   the caller should have passed us an empty menu */
	NSMenu *appMenu;
	NSMenuItem *appMenuItem;

	appMenu = [[NSMenu alloc] initWithTitle:@""];
	[appMenu addItemWithTitle:@"About DOSBox-X" action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];

	appMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	[appMenuItem setSubmenu:appMenu];
	[((NSMenu*)nsMenu) addItem:appMenuItem];
	[appMenuItem release];
	[appMenu release];
#else
    /* Re-use the application menu from SDL1 */
    sdl1_hax_stock_macosx_menu_additem((NSMenu*)nsMenu);
#endif
}

extern int pause_menu_item_tag;
extern bool is_paused;
extern void PushDummySDL(void);
extern bool MAPPER_IsRunning(void);
extern bool GUI_IsRunning(void);

static DOSBoxMenu *altMenu = NULL;

void menu_macosx_set_menuobj(DOSBoxMenu *new_altMenu) {
    if (new_altMenu != NULL && new_altMenu != &mainMenu)
        altMenu = new_altMenu;
    else
        altMenu = NULL;
}

@implementation NSApplication (DOSBoxX)
- (void)DOSBoxXMenuAction:(id)sender
{
    if (altMenu != NULL) {
        altMenu->mainMenuAction([sender tag]);
    }
    else {
        if ((is_paused && pause_menu_item_tag != [sender tag]) || MAPPER_IsRunning() || GUI_IsRunning()) return;
        /* sorry! */
        mainMenu.mainMenuAction([sender tag]);
    }
}

- (void)DOSBoxXMenuActionMapper:(id)sender
{
    (void)sender;
    if (is_paused || MAPPER_IsRunning() || GUI_IsRunning()) return;
    extern void MAPPER_Run(bool pressed);
    MAPPER_Run(false);
}

- (void)DOSBoxXMenuActionCapMouse:(id)sender
{
    (void)sender;
    if (is_paused || MAPPER_IsRunning() || GUI_IsRunning()) return;
    extern void MapperCapCursorToggle(void);
    MapperCapCursorToggle();
}

- (void)DOSBoxXMenuActionCfgGUI:(id)sender
{
    (void)sender;
    if (is_paused || MAPPER_IsRunning() || GUI_IsRunning()) return;
    extern void GUI_Run(bool pressed);
    GUI_Run(false);
}

- (void)DOSBoxXMenuActionPause:(id)sender
{
    (void)sender;
    extern bool unpause_now;
    extern void PauseDOSBox(bool pressed);

    if (MAPPER_IsRunning() || GUI_IsRunning()) return;

    if (is_paused) {
        PushDummySDL();
        unpause_now = true;
    }
    else {
        PauseDOSBox(true);
    }
}
@end

bool has_touch_bar_support = false;

bool macosx_detect_nstouchbar(void) {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101202/* touch bar interface appeared in 10.12.2+ according to Apple */
    return (has_touch_bar_support = (NSClassFromString(@"NSTouchBar") != nil));
#else
    return false;
#endif
}

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101202/* touch bar interface appeared in 10.12.2+ according to Apple */
# if !defined(C_SDL2)
extern "C" void sdl1_hax_make_touch_bar_set_callback(NSTouchBar* (*newcb)(NSWindow*));
# endif

static NSTouchBarItemIdentifier TouchBarCustomIdentifier = @"com.dosbox-x.touchbar.custom";
static NSTouchBarItemIdentifier TouchBarMapperIdentifier = @"com.dosbox-x.touchbar.mapper";
static NSTouchBarItemIdentifier TouchBarCFGGUIIdentifier = @"com.dosbox-x.touchbar.cfggui";
static NSTouchBarItemIdentifier TouchBarHostKeyIdentifier = @"com.dosbox-x.touchbar.hostkey";
static NSTouchBarItemIdentifier TouchBarPauseIdentifier = @"com.dosbox-x.touchbar.pause";
static NSTouchBarItemIdentifier TouchBarCursorCaptureIdentifier = @"com.dosbox-x.touchbar.capcursor";

@interface DOSBoxXTouchBarDelegate : NSViewController
@end

@interface DOSBoxXTouchBarDelegate () <NSTouchBarDelegate,NSTextViewDelegate>
@end

@interface DOSBoxHostButton : NSButton
@end
#endif

extern void ext_signal_host_key(bool enable);

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101202/* touch bar interface appeared in 10.12.2+ according to Apple */
@implementation DOSBoxHostButton
- (void)touchesBeganWithEvent:(NSEvent*)event
{
    fprintf(stderr,"Host key down\n");
    ext_signal_host_key(true);
    [super touchesBeganWithEvent:event];
}

- (void)touchesEndedWithEvent:(NSEvent*)event
{
    fprintf(stderr,"Host key up\n");
    ext_signal_host_key(false);
    [super touchesEndedWithEvent:event];
}

- (void)touchesCancelledWithEvent:(NSEvent*)event
{
    fprintf(stderr,"Host key cancelled\n");
    ext_signal_host_key(false);
    [super touchesEndedWithEvent:event];
}
@end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101202/* touch bar interface appeared in 10.12.2+ according to Apple */
@implementation DOSBoxXTouchBarDelegate
- (void)onHostKey:(id)sender
{
    (void)sender;
    fprintf(stderr,"HostKey\n");
}

- (NSTouchBarItem *)touchBar:(NSTouchBar *)touchBar makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier {
    (void)touchBar;

    if ([identifier isEqualToString:TouchBarMapperIdentifier]) {
        NSCustomTouchBarItem *item = [[NSCustomTouchBarItem alloc] initWithIdentifier:TouchBarMapperIdentifier];

        item.view = [NSButton buttonWithTitle:@"Mapper" target:NSApp action:@selector(DOSBoxXMenuActionMapper:)];
        item.customizationLabel = TouchBarCustomIdentifier;

        return item;
    }
    else if ([identifier isEqualToString:TouchBarHostKeyIdentifier]) {
        NSCustomTouchBarItem *item = [[NSCustomTouchBarItem alloc] initWithIdentifier:TouchBarHostKeyIdentifier];

        item.view = [DOSBoxHostButton buttonWithTitle:@"Host Key" target:self action:@selector(onHostKey:)];
        item.customizationLabel = TouchBarCustomIdentifier;

        return item;
    }
    else if ([identifier isEqualToString:TouchBarCFGGUIIdentifier]) {
        NSCustomTouchBarItem *item = [[NSCustomTouchBarItem alloc] initWithIdentifier:TouchBarCFGGUIIdentifier];

        item.view = [NSButton buttonWithTitle:@"Cfg GUI" target:NSApp action:@selector(DOSBoxXMenuActionCfgGUI:)];
        item.customizationLabel = TouchBarCustomIdentifier;

        return item;
    }
    else if ([identifier isEqualToString:TouchBarPauseIdentifier]) {
        NSCustomTouchBarItem *item = [[NSCustomTouchBarItem alloc] initWithIdentifier:TouchBarPauseIdentifier];

        item.view = [NSButton buttonWithImage:[NSImage imageNamed:NSImageNameTouchBarPauseTemplate] target:NSApp action:@selector(DOSBoxXMenuActionPause:)];
        item.customizationLabel = TouchBarCustomIdentifier;

        return item;
    }
    else if ([identifier isEqualToString:TouchBarCursorCaptureIdentifier]) {
        NSCustomTouchBarItem *item = [[NSCustomTouchBarItem alloc] initWithIdentifier:TouchBarCursorCaptureIdentifier];

        item.view = [NSButton buttonWithTitle:@"CapMouse" target:NSApp action:@selector(DOSBoxXMenuActionCapMouse:)];
        item.customizationLabel = TouchBarCustomIdentifier;

        return item;
    }
    else {
        fprintf(stderr,"Touch bar warning, unknown item '%s'\n",[identifier UTF8String]);
    }

    return nil;
}
@end
#endif

void macosx_reload_touchbar(void) {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101202/* touch bar interface appeared in 10.12.2+ according to Apple */
    NSWindow *wnd = nil;

# if !defined(C_SDL2)
    wnd = sdl1_hax_get_window();
# endif

    if (wnd != nil) {
        [wnd setTouchBar:nil];
    }
#endif
}

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101202/* touch bar interface appeared in 10.12.2+ according to Apple */
NSTouchBar* macosx_on_make_touch_bar(NSWindow *wnd) {
    (void)wnd;

    NSTouchBar* touchBar = [[NSTouchBar alloc] init];
    touchBar.delegate = [DOSBoxXTouchBarDelegate alloc];

    touchBar.customizationIdentifier = TouchBarCustomIdentifier;
    if (GUI_IsRunning()) {
        touchBar.defaultItemIdentifiers = @[
            NSTouchBarItemIdentifierOtherItemsProxy
        ];
    }
    else if (MAPPER_IsRunning()) {
        touchBar.defaultItemIdentifiers = @[
            NSTouchBarItemIdentifierFixedSpaceLarge, // try to keep the user from hitting the ESC button accidentally when reaching for Host Key
            TouchBarHostKeyIdentifier,
            NSTouchBarItemIdentifierFixedSpaceLarge,
            NSTouchBarItemIdentifierOtherItemsProxy
        ];
    }
    else {
        touchBar.defaultItemIdentifiers = @[
            NSTouchBarItemIdentifierFixedSpaceLarge, // try to keep the user from hitting the ESC button accidentally when reaching for Host Key
            TouchBarHostKeyIdentifier,
            NSTouchBarItemIdentifierFixedSpaceLarge,
            TouchBarPauseIdentifier,
            NSTouchBarItemIdentifierFixedSpaceLarge,
            TouchBarCursorCaptureIdentifier,
            NSTouchBarItemIdentifierFixedSpaceLarge,
            TouchBarMapperIdentifier,
            TouchBarCFGGUIIdentifier,
            NSTouchBarItemIdentifierOtherItemsProxy
        ];
    }

    touchBar.customizationAllowedItemIdentifiers = @[
        TouchBarHostKeyIdentifier,
        TouchBarMapperIdentifier,
        TouchBarCFGGUIIdentifier,
        TouchBarCursorCaptureIdentifier,
        TouchBarPauseIdentifier
    ];

// Do not mark as principal, it just makes the button centered in the touch bar
//    touchBar.principalItemIdentifier = TouchBarMapperIdentifier;

    return touchBar;
}
#endif

void macosx_init_touchbar(void) {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101202/* touch bar interface appeared in 10.12.2+ according to Apple */
# if !defined(C_SDL2)
    if (has_touch_bar_support)
        sdl1_hax_make_touch_bar_set_callback(macosx_on_make_touch_bar);
# endif
#endif
}

#if !defined(C_SDL2)
extern "C" void sdl1_hax_set_dock_menu(NSMenu *menu);
#endif

void macosx_init_dock_menu(void) {
#if !defined(C_SDL2)
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];

    {
        NSString *title = [[NSString alloc] initWithUTF8String: "Mapper editor"];
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:@selector(DOSBoxXMenuActionMapper:) keyEquivalent:@""];
        [menu addItem:item];
        [title release];
        [item release];
    }

    {
        NSString *title = [[NSString alloc] initWithUTF8String: "Configuration tool"];
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:@selector(DOSBoxXMenuActionCfgGUI:) keyEquivalent:@""];
        [menu addItem:item];
        [title release];
        [item release];
    }

    {
	    NSMenuItem *item = [NSMenuItem separatorItem];
        [menu addItem:item];
        [item release];
    }

    {
        NSString *title = [[NSString alloc] initWithUTF8String: "Pause emulation"];
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:@selector(DOSBoxXMenuActionPause:) keyEquivalent:@""];
        [menu addItem:item];
        [title release];
        [item release];
    }

    sdl1_hax_set_dock_menu(menu);

    [menu release];
#endif
}
#endif

#if !defined(C_SDL2)
extern "C" int sdl1_hax_macosx_window_to_monitor_and_update(CGDirectDisplayID *did);
#endif

int my_quartz_match_window_to_monitor(CGDirectDisplayID *new_id,NSWindow *wnd);

void MacOSX_GetWindowDPI(ScreenSizeInfo &info) {
    NSWindow *wnd = nil;

    info.clear();

#if !defined(C_SDL2)
    wnd = sdl1_hax_get_window();
#else
    SDL_Window* GFX_GetSDLWindow(void);

    SDL_SysWMinfo wminfo;
    memset(&wminfo,0,sizeof(wminfo));
    SDL_VERSION(&wminfo.version);

    if (SDL_GetWindowWMInfo(GFX_GetSDLWindow(),&wminfo) >= 0) {
        if (wminfo.subsystem == SDL_SYSWM_COCOA && wminfo.info.cocoa.window != NULL) {
            wnd = wminfo.info.cocoa.window;
        }
    }
#endif

    if (wnd != nil) {
        CGDirectDisplayID did = 0;

        if (my_quartz_match_window_to_monitor(&did,wnd) >= 0) {
            CGRect drct = CGDisplayBounds(did);
            CGSize dsz = CGDisplayScreenSize(did);

            info.method = METHOD_COREGRAPHICS;

            info.screen_position_pixels.x        = drct.origin.x;
            info.screen_position_pixels.y        = drct.origin.y;

            info.screen_dimensions_pixels.width  = drct.size.width;
            info.screen_dimensions_pixels.height = drct.size.height;

            /* According to Apple documentation, this function CAN return zero */
            if (dsz.width > 0 && dsz.height > 0) {
                info.screen_dimensions_mm.width      = dsz.width;
                info.screen_dimensions_mm.height     = dsz.height;

                if (info.screen_dimensions_mm.width > 0)
                    info.screen_dpi.width =
                        ((((double)info.screen_dimensions_pixels.width) * 25.4) /
                         ((double)info.screen_dimensions_mm.width));

                if (info.screen_dimensions_mm.height > 0)
                    info.screen_dpi.height =
                        ((((double)info.screen_dimensions_pixels.height) * 25.4) /
                         ((double)info.screen_dimensions_mm.height));
            }
        }
    }
}

int my_quartz_match_window_to_monitor(CGDirectDisplayID *new_id,NSWindow *wnd) {
    if (wnd != nil) {
        CGError err;
        uint32_t cnt = 1;
        CGDirectDisplayID did = 0;
        NSRect rct = [wnd frame];
// NTS: This did not appear until Mojave, and some followers on Github prefer to compile for somewhat older versions of OS X
//      NSPoint pt = [wnd convertPointToScreen:NSMakePoint(rct.size.width / 2, rct.size.height / 2)];
// NTS: convertRectToScreen however is documented to exist since 10.7, unless Apple got that wrong too...
        NSPoint pt = [wnd convertRectToScreen:NSMakeRect(rct.size.width / 2, rct.size.height / 2, 0, 0)].origin; /* x,y,w,h */

        {
            /* Eugh this ugliness wouldn't be necessary if we didn't have to fudge relative to primary display. */
            CGRect prct = CGDisplayBounds(CGMainDisplayID());
            pt.y = (prct.origin.y + prct.size.height) - pt.y;
        }

        err = CGGetDisplaysWithPoint(pt,1,&did,&cnt);

        /* This might happen if our window is so far off the screen that the center point does not match any monitor */
        if (err != kCGErrorSuccess) {
            err = kCGErrorSuccess;
            did = CGMainDisplayID(); /* Can't fail, eh, Apple? OK then. */
        }

        if (err == kCGErrorSuccess) {
            *new_id = did;
            return 0;
        }
    }

    return -1;
}

#if !defined(C_SDL2)
extern "C" int (*sdl1_hax_quartz_match_window_to_monitor)(CGDirectDisplayID *new_id,NSWindow *wnd);
#endif

void qz_set_match_monitor_cb(void) {
#if !defined(C_SDL2)
    sdl1_hax_quartz_match_window_to_monitor = my_quartz_match_window_to_monitor;
#endif
}

// WARNING! You must initialize the SDL Video subsystem *FIRST*
// before calling this function, or else strange errors and
// malfunctions occur in the Cocoa framework (at least in Big Sur).
// You can quit the SDL Video subsystem and reinitialize later
// after this function is done.
std::string MacOSX_prompt_folder(const char *default_folder) {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    NSModalResponse r;
    std::string res;

    [panel setPrompt:@"Choose"];
    [panel setCanChooseFiles:false];
    [panel setCanChooseDirectories:true];
    [panel setAllowsMultipleSelection:false];
    [panel setMessage:@"Select folder where to run emulation, which will become DOSBox-X's working directory:"];
    [panel setCanCreateDirectories:true]; /* sure, why not? */
    if (default_folder != NULL) [panel setDirectoryURL:[NSURL fileURLWithPath:[NSString stringWithFormat:@"%s",default_folder]]];

    r = [panel runModal];
    if (r == NSFileHandlingPanelOKButton) {
        NSArray *urls = [panel URLs];
        if ([urls count] > 0) {
            NSURL *url = urls[0];
            if ([[url scheme] isEqual: @"file"]) {
                /* NTS: /path/to/file is returned as file:///path/to/file */
                res = [[url relativePath] UTF8String];
            }
            else {
                fprintf(stderr,"WARNING: Rejecting returned protocol '%s', no selection accepted\n",
                    [[url scheme] UTF8String]);
            }
        }
    }

    [panel release];

    return res;
}

void MacOSX_alert(const char *title, const char *message) {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:[NSString stringWithFormat:@"%s",title]];
    [alert setInformativeText:[NSString stringWithFormat:@"%s",message]];
    [alert setAlertStyle:NSInformationalAlertStyle];
    [alert runModal];
}

int MacOSX_yesnocancel(const char *title, const char *message) {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert addButtonWithTitle:@"Yes"];
    [alert addButtonWithTitle:@"No"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert setMessageText:[NSString stringWithFormat:@"%s",title]];
    [alert setInformativeText:[NSString stringWithFormat:@"%s",message]];
    [alert setAlertStyle:NSInformationalAlertStyle];
    return [alert runModal];
}
