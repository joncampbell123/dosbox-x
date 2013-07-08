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

/* $Id: hardware.h,v 1.17 2009-06-23 17:46:05 c2woody Exp $ */

#ifndef DOSBOX_HARDWARE_H
#define DOSBOX_HARDWARE_H

#include <stdio.h>

class Section;
enum OPL_Mode {
	OPL_none,OPL_cms,OPL_opl2,OPL_dualopl2,OPL_opl3
};
#define CAPTURE_WAVE	0x01
#define CAPTURE_OPL		0x02
#define CAPTURE_MIDI	0x04
#define CAPTURE_IMAGE	0x08
#define CAPTURE_VIDEO	0x10

extern Bitu CaptureState;

void OPL_Init(Section* sec,OPL_Mode mode);
void CMS_Init(Section* sec);
void OPL_ShutDown(Section* sec);
void CMS_ShutDown(Section* sec);

bool SB_Get_Address(Bitu& sbaddr, Bitu& sbirq, Bitu& sbdma);
bool TS_Get_Address(Bitu& tsaddr, Bitu& tsirq, Bitu& tsdma);

extern Bit8u adlib_commandreg;
FILE * OpenCaptureFile(const char * type,const char * ext);

void CAPTURE_AddWave(Bit32u freq, Bit32u len, Bit16s * data);
#define CAPTURE_FLAG_DBLW	0x1
#define CAPTURE_FLAG_DBLH	0x2
void CAPTURE_AddImage(Bitu width, Bitu height, Bitu bpp, Bitu pitch, Bitu flags, float fps, Bit8u * data, Bit8u * pal);
void CAPTURE_AddMidi(bool sysex, Bitu len, Bit8u * data);

#endif
