/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "dosbox.h"
#include "control.h"
#include "hardware.h"
#include "setup.h"
#include "support.h"
#include "mem.h"
#include "mapper.h"
#include "pic.h"
#include "mixer.h"
#include "render.h"
#include "cross.h"

#include "rawint.h"

#include <map>

/* FIXME: This needs to be an enum */
bool native_zmbv = false;

std::string capturedir;
extern const char* RunningProgram;

unsigned int GFX_GetBShift();

MixerChannel * MIXER_FirstChannel(void);

void HARDWARE_Destroy(Section * sec) {
    (void)sec;//UNUSED
}

void HARDWARE_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("HARDWARE_Init: initializing");

	/* TODO: Hardware init. We moved capture init to it's own function. */
	AddExitFunction(AddExitFunctionFuncPair(HARDWARE_Destroy),true);
}

