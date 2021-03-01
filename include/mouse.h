/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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



#ifndef DOSBOX_MOUSE_H
#define DOSBOX_MOUSE_H

enum MOUSE_EMULATION
{
    MOUSE_EMULATION_NEVER,
    MOUSE_EMULATION_ALWAYS,
    MOUSE_EMULATION_INTEGRATION,
    MOUSE_EMULATION_LOCKED,
};

bool Mouse_SetPS2State(bool use);

void Mouse_ChangePS2Callback(uint16_t pseg, uint16_t pofs);


void Mouse_CursorMoved(float xrel,float yrel,float x,float y,bool emulate);
#if defined(WIN32) || defined(MACOSX) || defined(C_SDL2)
const char* Mouse_GetSelected(int x1, int y1, int x2, int y2, int w, int h, uint16_t *textlen);
void Mouse_Select(int x1, int y1, int x2, int y2, int w, int h, bool select);
#endif
void Mouse_ButtonPressed(uint8_t button);
void Mouse_ButtonReleased(uint8_t button);

void Mouse_AutoLock(bool enable);
bool Mouse_IsLocked();
void Mouse_BeforeNewVideoMode(bool setmode);
void Mouse_AfterNewVideoMode(bool setmode);

#endif
