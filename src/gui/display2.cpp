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


#include <curses.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#endif
#include "dosbox.h"
#include "paging.h"
#include "inout.h"
#include "pic.h"

#define REFRESH_DELAY 1000.0/50.0

struct {
	uint8_t* memory;
	uint8_t* render;
	bool update;
	uint8_t index;
	struct {
		bool enable;
		bool update;
		uint8_t sline;
		uint8_t eline;
		uint16_t pos;
	} cursor;
} disp2;

static bool disp2_init=false;

bool DISP2_Active(void) {
	return disp2_init;
}

static void DISP2_WriteChar(uint16_t row,uint16_t col,uint16_t chat) {
	uint8_t ch=(uint8_t)(chat&0xff),at=(uint8_t)(chat>>8);
	if (GCC_UNLIKELY((at&0x77)==0)) attrset(COLOR_PAIR(1) | A_INVIS);
	else if ((at&0x77)==0x70) attrset(COLOR_PAIR(1) | A_REVERSE);
	else if (at&8) attrset(COLOR_PAIR(1) | A_BOLD);
	else attrset(COLOR_PAIR(1) | A_NORMAL);
	mvaddch(row,col,ch);
}

#define new_pos disp2.cursor.pos

static void DISP2_Refresh(Bitu /*val*/) {
	static Bitu blink=0;
	static uint16_t old_pos=0;
	static uint8_t map_ch=0,old_ch=0;
	bool need_refresh=false;
	if (disp2.update) {
		for (uint16_t addr=0;addr<4000;addr+=2) {
			uint16_t memory=host_readw(&disp2.memory[addr]);
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
	uint8_t new_ch=(++blink&8)?map_ch:0;
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
	uint8_t readb(PhysPt addr) {
		return disp2.memory[addr&0xfff];
	}
	void writeb(PhysPt addr,uint8_t val) {
		disp2.memory[addr&0xfff]=val;
		disp2.update=true;
	}
};

static DISP2_PageHandler disp2_page_handler;

void DISP2_SetPageHandler(void) {
	if (machine!=MCH_HERC && machine!=MCH_MDA && disp2_init) MEM_SetPageHandler(0xB0,8,&disp2_page_handler);
}

static void DISP2_write_crtc_index(Bitu /*port*/,Bitu val,Bitu /*iolen*/) {
	disp2.index=(uint8_t)(val&0x1f);
}

static void DISP2_write_crtc_data(Bitu /*port*/,Bitu val,Bitu /*iolen*/) {
	switch (disp2.index) {
	case 0x0a: // cursor start
		disp2.cursor.enable=((val&0x60)!=0x20);
		disp2.cursor.sline=(uint8_t)(val&0x1f);
		disp2.cursor.update=true;
		break;
	case 0x0b: // cursor end
		disp2.cursor.eline=(uint8_t)(val&0x1f);
		disp2.cursor.update=true;
		break;
	case 0x0e: // cursor position high
		disp2.cursor.pos=(disp2.cursor.pos&0x00ff)|(uint16_t)((val&0x0f)<<8);
		break;
	case 0x0f: // cursor position low
		disp2.cursor.pos=(disp2.cursor.pos&0xff00)|(uint16_t)val;
		break;
	}
}

static Bitu DISP2_read_crtc_data(Bitu /*port*/,Bitu /*iolen*/) {
	switch (disp2.index) {
	case 0x0e: // cursor position high
		return (uint8_t)(disp2.cursor.pos>>8);
	case 0x0f: // cursor position low
		return (uint8_t)(disp2.cursor.pos&0xff);
	default:
		return (Bitu)(~0);
	}
}

static Bitu DISP2_read_status(Bitu /*port*/,Bitu /*iolen*/) {
	static uint8_t status=0xf0;
	status^=0x09; // toggle stream and hsync bits for detection
	return status;
}

void DISP2_RegisterPorts(void) {
	if (machine!=MCH_HERC && machine!=MCH_MDA && disp2_init) {
		for (Bitu i=0;i<4;i++) {
			IO_RegisterWriteHandler(0x3b0+i*2,DISP2_write_crtc_index,IO_MB);
			IO_RegisterWriteHandler(0x3b0+i*2+1,DISP2_write_crtc_data,IO_MB);
			IO_RegisterReadHandler(0x3b0+i*2+1,DISP2_read_crtc_data,IO_MB);
		}
		IO_RegisterReadHandler(0x3ba,DISP2_read_status,IO_MB);
		PIC_AddEvent(DISP2_Refresh,REFRESH_DELAY);
	}
}

#ifdef WIN32
static void DISP2_ResizeConsole( HANDLE hConsole, SHORT xSize, SHORT ySize ) {   
	CONSOLE_SCREEN_BUFFER_INFO csbi; // Hold Current Console Buffer Info 
	BOOL bSuccess;   
	SMALL_RECT srWindowRect;         // Hold the New Console Size 
	COORD coordScreen;    

	bSuccess = GetConsoleScreenBufferInfo( hConsole, &csbi );

	// Get the Largest Size we can size the Console Window to 
	coordScreen = GetLargestConsoleWindowSize( hConsole );

	// Define the New Console Window Size and Scroll Position 
	srWindowRect.Right  = (SHORT)(std::min(xSize, coordScreen.X) - 1);
	srWindowRect.Bottom = (SHORT)(std::min(ySize, coordScreen.Y) - 1);
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
#endif

void DISP2_Init(uint8_t color) {
	// initialize console
#ifdef WIN32
	AllocConsole();
	SetConsoleTitle("DOSBox-X Secondary Display");
	DISP2_ResizeConsole(GetStdHandle(STD_OUTPUT_HANDLE),80,25);
#endif
	// initialize curses
	initscr();
	resize_term(25,80);
	curs_set(0);
	start_color();
	if (!has_colors()) return;
	switch (color) {
	case 1: // amber
		if (can_change_color()) {
			init_color(7,667,333,0);
			init_color(15,1000,667,0);
			init_pair(1,7,COLOR_BLACK);
		} else init_pair(1,COLOR_YELLOW,COLOR_BLACK);
		break;
	case 2: // green
		if (can_change_color()) {
			init_color(7,0,500,0);
			init_color(15,0,1000,0);
			init_pair(1,7,COLOR_BLACK);
		} else init_pair(1,COLOR_GREEN,COLOR_BLACK);
		break;
	default: // white
		if (can_change_color()) {
			init_color(7,667,667,667);
			init_color(15,1000,1000,1000);
			init_pair(1,7,COLOR_BLACK);
		} else init_pair(1,COLOR_WHITE,COLOR_BLACK);
		break;
	}
	// initialize data
	memset(&disp2,0,sizeof(disp2));
	disp2.memory=new uint8_t[4096];
	memset(disp2.memory,0,4096);
	disp2.render=new uint8_t[4000];
	memset(disp2.render,0,4000);
	// initialization complete
	disp2_init=true;
}
