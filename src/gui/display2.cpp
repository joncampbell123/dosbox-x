/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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

#if defined(WIN32) && !(C_DEBUG)

#include <curses.h>
#include <windows.h>
#include "dosbox.h"
#include "paging.h"
#include "inout.h"
#include "pic.h"

#define REFRESH_DELAY 1000.0/50.0

struct {
	Bit8u* memory;
	Bit8u* render;
	bool update;
	Bit8u index;
	struct {
		bool enable;
		bool update;
		Bit8u sline;
		Bit8u eline;
		Bit16u pos;
	} cursor;
} disp2;

static bool disp2_init=false;

bool DISP2_Active(void) {
	return disp2_init;
}

static void DISP2_WriteChar(Bit16u row,Bit16u col,Bit16u chat) {
	Bit8u ch=(Bit8u)(chat&0xff),at=(Bit8u)(chat>>8);
	if (GCC_UNLIKELY((at&0x77)==0)) attrset(COLOR_PAIR(1) | A_INVIS);
	else if ((at&0x77)==0x70) attrset(COLOR_PAIR(1) | A_REVERSE);
	else if (at&8) attrset(COLOR_PAIR(1) | A_BOLD);
	else attrset(COLOR_PAIR(1) | A_NORMAL);
	mvaddrawch(row,col,ch);
}

#define new_pos disp2.cursor.pos

static void DISP2_Refresh(Bitu /*val*/) {
	static Bitu blink=0;
	static Bit16u old_pos=0;
	static Bit8u map_ch=0,old_ch=0;
	bool need_refresh=false;
	if (disp2.update) {
		for (Bit16u addr=0;addr<4000;addr+=2) {
			Bit16u memory=host_readw(&disp2.memory[addr]);
			if (memory!=host_readw(&disp2.render[addr])) {
				DISP2_WriteChar(addr/160,(addr%160)>>1,memory);
				host_writew(&disp2.render[addr],memory);
			}
		}
		disp2.update=false;
		need_refresh=true;
	}
	if (disp2.cursor.update) {
		if (!disp2.cursor.enable || disp2.cursor.eline<disp2.cursor.sline) {
			map_ch=0; // cursor off
		} else if (disp2.cursor.sline<4) {
			if (disp2.cursor.eline>10) map_ch=0xdb; // full block
			else map_ch=0xdf; // upper half block
		} else if (disp2.cursor.sline<10) {
			if (disp2.cursor.eline>12) map_ch=0xdc; // lower half block
			else map_ch=0x2d; // minus sign
		} else map_ch=0x5f; // underscore
		disp2.cursor.update=false;
	}
	Bit8u new_ch=(++blink&8)?map_ch:0;
	if ((old_ch) && old_pos<2000 && (new_ch==0 || new_pos!=old_pos)) {
		DISP2_WriteChar(old_pos/80,old_pos%80,host_readw(&disp2.memory[old_pos<<1]));
		need_refresh=true;
	}
	if ((new_ch) && new_pos<2000 && (new_ch!=old_ch || new_pos!=old_pos)) {
		DISP2_WriteChar(new_pos/80,new_pos%80,host_readw(&disp2.memory[new_pos<<1])&0xff00|new_ch);
		need_refresh=true;
	}
	old_ch=new_ch;
	old_pos=new_pos;
	if (need_refresh) refresh();
	PIC_AddEvent(DISP2_Refresh,REFRESH_DELAY);
}

class DISP2_PageHandler : public PageHandler {
public:
	DISP2_PageHandler() {
		flags=PFLAG_NOCODE;
	}
	// the 4kB map area is repeated in the 32kB range
	Bitu readb(PhysPt addr) {
		return disp2.memory[addr&0xfff];
	}
	void writeb(PhysPt addr,Bitu val) {
		disp2.memory[addr&0xfff]=(Bit8u)val;
		disp2.update=true;
	}
};

static DISP2_PageHandler disp2_page_handler;

void DISP2_SetPageHandler(void) {
	if (machine!=MCH_HERC && disp2_init) MEM_SetPageHandler(0xB0,8,&disp2_page_handler);
}

static void DISP2_write_crtc_index(Bitu /*port*/,Bitu val,Bitu /*iolen*/) {
	disp2.index=(Bit8u)(val&0x1f);
}

static void DISP2_write_crtc_data(Bitu /*port*/,Bitu val,Bitu /*iolen*/) {
	switch (disp2.index) {
	case 0x0a: // cursor start
		disp2.cursor.enable=((val&0x60)!=0x20);
		disp2.cursor.sline=(Bit8u)(val&0x1f);
		disp2.cursor.update=true;
		break;
	case 0x0b: // cursor end
		disp2.cursor.eline=(Bit8u)(val&0x1f);
		disp2.cursor.update=true;
		break;
	case 0x0e: // cursor position high
		disp2.cursor.pos=(disp2.cursor.pos&0x00ff)|(Bit16u)((val&0x0f)<<8);
		break;
	case 0x0f: // cursor position low
		disp2.cursor.pos=(disp2.cursor.pos&0xff00)|(Bit16u)val;
		break;
	}
}

static Bitu DISP2_read_crtc_data(Bitu /*port*/,Bitu /*iolen*/) {
	switch (disp2.index) {
	case 0x0e: // cursor position high
		return (Bit8u)(disp2.cursor.pos>>8);
	case 0x0f: // cursor position low
		return (Bit8u)(disp2.cursor.pos&0xff);
	default:
		return (Bitu)(~0);
	}
}

static Bitu DISP2_read_status(Bitu /*port*/,Bitu /*iolen*/) {
	static Bit8u status=0xf0;
	status^=0x09; // toggle stream and hsync bits for detection
	return status;
}

void DISP2_RegisterPorts(void) {
	if (machine!=MCH_HERC && disp2_init) {
		for (Bitu i=0;i<4;i++) {
			IO_RegisterWriteHandler(0x3b0+i*2,DISP2_write_crtc_index,IO_MB);
			IO_RegisterWriteHandler(0x3b0+i*2+1,DISP2_write_crtc_data,IO_MB);
			IO_RegisterReadHandler(0x3b0+i*2+1,DISP2_read_crtc_data,IO_MB);
		}
		IO_RegisterReadHandler(0x3ba,DISP2_read_status,IO_MB);
		PIC_AddEvent(DISP2_Refresh,REFRESH_DELAY);
	}
}

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

static void DISP2_ResizeConsole( HANDLE hConsole, SHORT xSize, SHORT ySize ) {   
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

void DISP2_Init(Bit8u color) {
	// initialize console
	extern bool no_stdout; no_stdout = true;
	FreeConsole(); AllocConsole();
	SetConsoleCP(437); SetConsoleOutputCP(437);
	SetConsoleTitle("DOSBox Secondary Display");
	DISP2_ResizeConsole(GetStdHandle(STD_OUTPUT_HANDLE),80,25);
	// initialize curses
	initscr();
	resize_term(25,80);
	curs_set(0);
	start_color();
	if (!has_colors()) return;
	switch (color) {
	case 1: // amber
		/*if (can_change_color()) {
			init_color(7,667,333,0);
			init_color(15,1000,667,0);
			init_pair(1,7,COLOR_BLACK);
		} else*/ init_pair(1,COLOR_YELLOW,COLOR_BLACK);
		break;
	case 2: // green
		/*if (can_change_color()) {
			init_color(7,0,500,0);
			init_color(15,0,1000,0);
			init_pair(1,7,COLOR_BLACK);
		} else*/ init_pair(1,COLOR_GREEN,COLOR_BLACK);
		break;
	default: // white
		/*if (can_change_color()) {
			init_color(7,667,667,667);
			init_color(15,1000,1000,1000);
			init_pair(1,7,COLOR_BLACK);
		} else*/ init_pair(1,COLOR_WHITE,COLOR_BLACK);
		break;
	}
	// initialize data
	memset(&disp2,0,sizeof(disp2));
	disp2.memory=new Bit8u[4096];
	memset(disp2.memory,0,4096);
	disp2.render=new Bit8u[4000];
	memset(disp2.render,0,4000);
	// initialization complete
	disp2_init=true;
}

#endif //defined(WIN32) && !(C_DEBUG)
