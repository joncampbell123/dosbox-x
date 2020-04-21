/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */

#ifdef WIN32

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

/* 
	Have to remember where i ripped this code sometime ago.

*/
static void ResizeConsole( HANDLE hConsole, SHORT xSize, SHORT ySize ) {   
	CONSOLE_SCREEN_BUFFER_INFO csbi; // Hold Current Console Buffer Info 
	SMALL_RECT srWindowRect;         // Hold the New Console Size 
	COORD coordScreen;    
	
	GetConsoleScreenBufferInfo( hConsole, &csbi );
	
	// Get the Largest Size we can size the Console Window to 
	coordScreen = GetLargestConsoleWindowSize( hConsole );
	
	// Define the New Console Window Size and Scroll Position 
	srWindowRect.Right  = (SHORT)(min(xSize, coordScreen.X) - 1);
	srWindowRect.Bottom = (SHORT)(min(ySize, coordScreen.Y) - 1);
	srWindowRect.Left   = srWindowRect.Top = (SHORT)0;
	
	// Define the New Console Buffer Size    
	coordScreen.X = xSize;
	coordScreen.Y = ySize;
	
	// If the Current Buffer is Larger than what we want, Resize the 
	// Console Window First, then the Buffer 
	if( (DWORD)csbi.dwSize.X * csbi.dwSize.Y > (DWORD) xSize * ySize)
	{
		SetConsoleWindowInfo( hConsole, TRUE, &srWindowRect );
		SetConsoleScreenBufferSize( hConsole, coordScreen );
	}
	
	// If the Current Buffer is Smaller than what we want, Resize the 
	// Buffer First, then the Console Window 
	if( (DWORD)csbi.dwSize.X * csbi.dwSize.Y < (DWORD) xSize * ySize )
	{
		SetConsoleScreenBufferSize( hConsole, coordScreen );
		SetConsoleWindowInfo( hConsole, TRUE, &srWindowRect );
	}
	
	// If the Current Buffer *is* the Size we want, Don't do anything!
	return;
   }


void WIN32_Console() {
	AllocConsole();
	SetConsoleTitle("DOSBox-X Debugger");
	ResizeConsole(GetStdHandle(STD_OUTPUT_HANDLE),80,50);
}
#endif
