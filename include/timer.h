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

#ifndef DOSBOX_TIMER_H
#define DOSBOX_TIMER_H

/* underlying clock rate in HZ */
#include <SDL.h>

#define PIT_TICK_RATE_IBM 1193182

#define PIT_TICK_RATE_PC98_8MHZ 1996800

#define PIT_TICK_RATE_PC98_10MHZ 2457600

extern unsigned long PIT_TICK_RATE;

#define GetTicks() SDL_GetTicks()

typedef void (*TIMER_TickHandler)(void);

/* Register a function that gets called everytime if 1 or more ticks pass */
void TIMER_AddTickHandler(TIMER_TickHandler handler);
void TIMER_DelTickHandler(TIMER_TickHandler handler);

/* This will add 1 milliscond to all timers */
void TIMER_AddTick(void);

/* Functions for the system control port 61h */
bool TIMER_GetOutput2();
void TIMER_SetGate2(bool in);

#endif
