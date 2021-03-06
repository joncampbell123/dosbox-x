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

#ifndef DOSBOX_LOGGING_H
#define DOSBOX_LOGGING_H

#include <stdio.h>
#include "setup.h"

enum LOG_TYPES {
	LOG_ALL,
	LOG_VGA, LOG_VGAGFX,LOG_VGAMISC,LOG_INT10,
	LOG_SB,LOG_DMACONTROL,
	LOG_FPU,LOG_CPU,LOG_PAGING,
	LOG_FCB,LOG_FILES,LOG_IOCTL,LOG_EXEC,LOG_DOSMISC,
	LOG_PIT,LOG_KEYBOARD,LOG_PIC,
	LOG_MOUSE,LOG_BIOS,LOG_GUI,LOG_MISC,
	LOG_IO,
	LOG_PCI,
	LOG_VOODOO,
	LOG_MAX
};

enum LOG_SEVERITIES {
	LOG_DEBUG,
	LOG_NORMAL,
	LOG_WARN,
	LOG_ERROR,
	LOG_FATAL,
	LOG_NEVER
};

struct _LogGroup {
	char const* front;
	enum LOG_SEVERITIES min_severity;
};

extern _LogGroup loggrp[LOG_MAX];
extern FILE* debuglog;

class LOG 
{ 
	LOG_TYPES       d_type;
	LOG_SEVERITIES  d_severity;
public:

	LOG (LOG_TYPES type , LOG_SEVERITIES severity):
		d_type(type),
		d_severity(severity)
		{}

	static void ParseEnableSetting(_LogGroup &group,const char *setting);
	static void SetupConfigSection(void);
	static void EarlyInit();
	static void Init();
	static void Exit();

	void operator() (char const* format, ...) GCC_ATTRIBUTE(__format__(__printf__, 2, 3));  //../src/debug/debug_gui.cpp
};

void					DEBUG_ShowMsg(char const* format,...) GCC_ATTRIBUTE(__format__(__printf__, 1, 2));

#define LOG_MSG				DEBUG_ShowMsg

#endif //DOSBOX_LOGGING_H

