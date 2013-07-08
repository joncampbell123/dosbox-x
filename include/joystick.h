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

/* $Id: joystick.h,v 1.13 2009-05-27 09:15:41 qbix79 Exp $ */
#ifndef DOSBOX_JOYSTICK_H
#define DOSBOX_JOYSTICK_H
void JOYSTICK_Enable(Bitu which,bool enabled);

void JOYSTICK_Button(Bitu which,Bitu num,bool pressed);

void JOYSTICK_Move_X(Bitu which,float x);

void JOYSTICK_Move_Y(Bitu which,float y);

bool JOYSTICK_IsEnabled(Bitu which);

bool JOYSTICK_GetButton(Bitu which, Bitu num);

float JOYSTICK_GetMove_X(Bitu which);

float JOYSTICK_GetMove_Y(Bitu which);

enum JoystickType {
	JOY_NONE,
	JOY_AUTO,
	JOY_2AXIS,
	JOY_4AXIS,
	JOY_4AXIS_2,
	JOY_FCS,
	JOY_CH
};

extern JoystickType joytype;
extern bool button_wrapping_enabled;
#endif
