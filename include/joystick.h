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
