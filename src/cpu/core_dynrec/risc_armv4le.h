/*
 *  Copyright (C) 2002-2018  The DOSBox Team
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



/* ARMv4/ARMv7 (little endian) backend (switcher) by M-HT */

#include "risc_armv4le-common.h"

// choose your destiny:
#if C_TARGETCPU == ARMV7LE
	#include "risc_armv4le-o3.h"
#else
	#if defined(__THUMB_INTERWORK__)
		#include "risc_armv4le-thumb-iw.h"
	#else
		#include "risc_armv4le-o3.h"
//		#include "risc_armv4le-thumb-niw.h"
//		#include "risc_armv4le-thumb.h"
	#endif
#endif
