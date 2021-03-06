/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef __WIN32__
#include <windows.h>
#include <windowsx.h>
#include "resource.h"
#endif

#define menu_compatible menu.compatible
#define menu_startup menu.startup
#define menu_gui menu.gui

#ifndef MENUDEF_H
#define MENUDEF_H

struct MENU_Block {
	bool toggle;      // toggle menu bar
	bool startup;     // verify if DOSBox is started with menu patch
	bool hidecycles;  // toggle cycles, fps, cpu usage information on title bar
    bool showrt;      // show realtime percentage
	bool boot;        // verify if boot is being used (if enabled, it is unable to mount drives)
	bool gui;         // enable or disable gui system (if disabled, it is unable to use/toggle menu bar)
	bool resizeusing; // check if resizable window can be used
	bool compatible;  // compatible mode for win9x/2000 (if enabled, GUI system will be disabled)
	bool maxwindow; // check window state
	MENU_Block():toggle(false),startup(false),hidecycles(false),showrt(false),boot(false),gui(true),resizeusing(false),compatible(false),maxwindow(false){ }
};
extern MENU_Block menu;

#endif /* MENUDEF_H */

