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
	BOOL bSuccess;   
	SMALL_RECT srWindowRect;         // Hold the New Console Size 
	COORD coordScreen;    
	
	bSuccess = GetConsoleScreenBufferInfo( hConsole, &csbi );
	
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
		bSuccess = SetConsoleWindowInfo( hConsole, TRUE, &srWindowRect );
		bSuccess = SetConsoleScreenBufferSize( hConsole, coordScreen );
	}
	
	// If the Current Buffer is Smaller than what we want, Resize the 
	// Buffer First, then the Console Window 
	if( (DWORD)csbi.dwSize.X * csbi.dwSize.Y < (DWORD) xSize * ySize )
	{
		bSuccess = SetConsoleScreenBufferSize( hConsole, coordScreen );
		bSuccess = SetConsoleWindowInfo( hConsole, TRUE, &srWindowRect );
	}
	
	// If the Current Buffer *is* the Size we want, Don't do anything! 
	return;
   }


void WIN32_Console() {
	AllocConsole();
	SetConsoleTitle("DOSBox Debugger");
	ResizeConsole(GetStdHandle(STD_OUTPUT_HANDLE),80,50);
}
#endif
