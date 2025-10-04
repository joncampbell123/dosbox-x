/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

/* Useful functions and variables from SDL_timer.c */
#include "SDL_timer.h"

#define ROUND_RESOLUTION(X)	\
	(((X+TIMER_RESOLUTION-1)/TIMER_RESOLUTION)*TIMER_RESOLUTION)

extern int SDL_timer_started;
extern int SDL_timer_running;

/* Data to handle a single periodic alarm */
extern Uint32 SDL_alarm_interval;
extern SDL_TimerCallback SDL_alarm_callback;

/* Set whether or not the timer should use a thread.
   This should be called while the timer subsystem is running.
*/
extern int SDL_SetTimerThreaded(int value);

extern int SDL_TimerInit(void);
extern void SDL_TimerQuit(void);

/* This function is called from the SDL event thread if it is available */
extern void SDL_ThreadedTimerCheck(void);
