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

/* Local Debug Function */


#if C_DEBUG
#include <curses.h>
#include "mem.h"

#define PAIR_BLACK_BLUE 1
#define PAIR_BYELLOW_BLACK 2
#define PAIR_GREEN_BLACK 3
#define PAIR_BLACK_GREY 4
#define PAIR_GREY_RED 5
#define PAIR_BLACK_GREEN 6

void DBGUI_StartUp(void);

class DBGBlock {
public:
	DBGBlock() : win_main(NULL), win_reg(NULL), win_data(NULL), win_code(NULL),
		win_var(NULL), win_out(NULL), active_win(0), input_y(0), global_mask(0) { }
public:
	WINDOW * win_main;					/* The Main Window */
	WINDOW * win_reg;					/* Register Window */
	WINDOW * win_data;					/* Data Output window */
	WINDOW * win_code;					/* Disassembly/Debug point Window */
	WINDOW * win_var;					/* Variable Window */
	WINDOW * win_out;					/* Text Output Window */
	Bit32u active_win;					/* Current active window */
	Bit32u input_y;
	Bit32u global_mask;					/* Current msgmask */
};


struct DASMLine {
	Bit32u pc;
	char dasm[80];
	PhysPt ea;
	Bit16u easeg;
	Bit32u eaoff;
};

extern DBGBlock dbg;

/* Local Debug Stuff */
Bitu DasmI386(char* buffer, PhysPt pc, Bitu cur_ip, bool bit32);
int  DasmLastOperandSize(void);
#endif

