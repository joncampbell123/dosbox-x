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

/* Local Debug Function */


#if C_DEBUG
#include <curses.h>
#include "mem.h"

#include <string>

#define PAIR_BLACK_BLUE 1
#define PAIR_BYELLOW_BLACK 2
#define PAIR_GREEN_BLACK 3
#define PAIR_BLACK_GREY 4
#define PAIR_GREY_RED 5
#define PAIR_BLACK_GREEN 6
#define PAIR_WHITE_BLUE 7

void DBGUI_StartUp(void);

extern const unsigned int dbg_def_win_height[];
extern const char *dbg_def_win_titles[];
extern const char *dbg_win_names[];

class DBGBlock {
public:
    enum {
        DATV_SEGMENTED=0,
        DATV_VIRTUAL,
        DATV_PHYSICAL
    };
    enum {
        /* main not counted */
        WINI_REG,
        WINI_DATA,
        WINI_CODE,
        WINI_VAR,
        WINI_OUT,
        /* inp not counted */

        WINI_MAX_INDEX
    };
    bool win_vis[WINI_MAX_INDEX] = {};
    std::string win_title[WINI_MAX_INDEX];
    unsigned char win_order[WINI_MAX_INDEX] = {};
    unsigned int win_height[WINI_MAX_INDEX] = {};
public:
	DBGBlock() : win_main(NULL), win_reg(NULL), win_data(NULL), win_code(NULL),
		win_var(NULL), win_out(NULL), win_inp(NULL), active_win(WINI_CODE), input_y(0), global_mask(0), data_view(0xFF) {
        for (unsigned int i=0;i < WINI_MAX_INDEX;i++) {
            win_height[i] = dbg_def_win_height[i];
            win_title[i] = dbg_def_win_titles[i];
            win_vis[i] = (i != WINI_VAR);
            win_order[i] = i;
        }
    }
public:
	WINDOW * win_main;					/* The Main Window (not counted in tab enumeration) */

	WINDOW * win_reg;					/* Register Window */
	WINDOW * win_data;					/* Data Output window */
	WINDOW * win_code;					/* Disassembly/Debug point Window */
	WINDOW * win_var;					/* Variable Window */
	WINDOW * win_out;					/* Text Output Window */

    WINDOW * win_inp;                   /* Input window (not counted in tab enumeration) */

    Bit32u active_win;					/* Current active window */
	Bit32u input_y;
	Bit32u global_mask;					/* Current msgmask */

    unsigned char data_view;

    void set_data_view(unsigned int view);

    WINDOW *get_win(int idx);
    WINDOW* &get_win_ref(int idx);
    const char *get_winname(int idx);
    const char *get_wintitle(int idx);
    std::string windowlist_by_name(void);
    int name_to_win(const char *name);
    WINDOW *get_active_win(void);
    int win_find_order(int wnd);
    int win_prev_by_order(int order);
    int win_next_by_order(int order);
    void swap_order(int o1,int o2);
    void next_window(void);
};


struct DASMLine {
	Bit32u pc;
	char dasm[80];
	PhysPt ea;
	uint16_t easeg;
	Bit32u eaoff;
};

extern DBGBlock dbg;

/* Local Debug Stuff */
Bitu DasmI386(char* buffer, PhysPt pc, Bit32u cur_ip, bool bit32);
int  DasmLastOperandSize(void);
#endif

