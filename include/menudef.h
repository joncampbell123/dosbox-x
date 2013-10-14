/*
 *  Copyright (C) 2002-2013  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef __WIN32__
#include <windows.h>
#include <windowsx.h>
#include "resource.h"

#define menu_compatible menu.compatible
#define menu_gui menu.gui
#define menu_startup menu.startup
#else

// If these are used, the optimizer can completely remove code that is not
// needed on Linux. This way, code is less cluttered with #ifdefs
#define menu_compatible (false)
#define menu_gui (false)
#define menu_startup (false)
#endif

struct MENU_Block {
	bool toggle;      // toggle menu bar
	bool startup;     // verify if DOSBox is started with menu patch
	bool hidecycles;  // toggle cycles, fps, cpu usage information on title bar
	bool boot;        // verify if boot is being used (if enabled, it is unable to mount drives)
	bool gui;         // enable or disable gui system (if disabled, it is unable to use/toggle menu bar)
	bool resizeusing; // check if resizable window can be used
	bool compatible;  // compatible mode for win9x/2000 (if enabled, GUI system will be disabled)
	bool maxwindow; // check window state
	MENU_Block():toggle(false),startup(false),hidecycles(false),boot(false),resizeusing(false),gui(true),compatible(false),maxwindow(false){ }
};
extern MENU_Block menu;

