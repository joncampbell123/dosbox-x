/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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

/* $Id: risc_armv4le.h,v 1.3 2009-05-27 09:15:41 qbix79 Exp $ */


/* ARMv4 (little endian) backend (switcher) by M-HT */

#include "risc_armv4le-common.h"

// choose your destiny:
#include "risc_armv4le-thumb-niw.h"
//#include "risc_armv4le-thumb-iw.h"
//#include "risc_armv4le-thumb.h"
//#include "risc_armv4le-s3.h"
//#include "risc_armv4le-o3.h"
