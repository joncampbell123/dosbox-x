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


#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fstream>

#if defined(WIN32)
#include <conio.h>
#include <SDL.h>
#endif

#include "dosbox.h"
#include "logging.h"
#include "support.h"
#include "control.h"
#include "regs.h"
#include "menu.h"
#include "debug.h"
#include "debug_inc.h"

#include <stdexcept>
#include <exception>

using namespace std;

bool log_int21 = false;
bool log_fileio = false;

static bool has_LOG_Init = false;
static bool has_LOG_EarlyInit = false;
static bool do_LOG_stderr = false;

bool logBuffSuppressConsole = false;
bool logBuffSuppressConsoleNeedUpdate = false;

_LogGroup loggrp[LOG_MAX]={{"",LOG_NORMAL},{0,LOG_NORMAL}};
FILE* debuglog = NULL;

#if C_DEBUG
static bool logBuffHasDiscarded = false;

#include <curses.h>

#include <list>
#include <string>
using namespace std;

std::string DBGBlock::windowlist_by_name(void) {
    std::string comp;
    unsigned int i;

    for (i=0;i < DBGBlock::WINI_MAX_INDEX;i++) {
        if (i != 0) comp += ",";
        comp += dbg_win_names[i];
    }

    return comp;
}

const unsigned int dbg_def_win_height[DBGBlock::WINI_MAX_INDEX] = {
    7,          /* WINI_REG */
    9,          /* WINI_DATA */
    12,         /* WINI_CODE */
    5,          /* WINI_VAR */
    6           /* WINI_OUT */
};

const char *dbg_def_win_titles[DBGBlock::WINI_MAX_INDEX] = {
    "Register Overview",        /* WINI_REG */
    "Data Overview",            /* WINI_DATA */
    "Code Overview",            /* WINI_CODE */
    "Variable Overview",        /* WINI_VAR */
    "Output"                    /* WINI_OUT */
};

const char *dbg_win_names[DBGBlock::WINI_MAX_INDEX] = {
    "REG",
    "DATA",
    "CODE",
    "VAR",
    "OUT"
};

#define MAX_LOG_BUFFER 4000
static list<string> logBuff;
static list<string>::iterator logBuffPos = logBuff.end();

extern int old_cursor_state;

bool savetologfile(const char *name) {
    std::ofstream out(name);
    if (!out.is_open()) return false;
    std::list<string>::iterator it;
    for (it = logBuff.begin(); it != logBuff.end(); ++it)
        out << (std::string)(*it) << endl;
    out.close();
    return true;
}

const char *DBGBlock::get_winname(int idx) {
    if (idx >= 0 && idx < DBGBlock::WINI_MAX_INDEX)
        return dbg_win_names[idx];

    return NULL;
}

const char *DBGBlock::get_wintitle(int idx) {
    if (idx >= 0 && idx < DBGBlock::WINI_MAX_INDEX)
        return dbg.win_title[idx].c_str();

    return NULL;
}

int DBGBlock::name_to_win(const char *name) {
    for (unsigned int i=0;i < DBGBlock::WINI_MAX_INDEX;i++) {
        if (!strcasecmp(name,dbg_win_names[i]))
            return (int)i;
    }

    return -1;
}

void DBGBlock::next_window(void) {
    int order = win_find_order((int)active_win);

    if (order < 0) order = 0;

    order = win_next_by_order(order);
    if (order >= 0) active_win = win_order[order];
}

void DBGBlock::swap_order(int o1,int o2) {
    if (o1 != o2)
        std::swap(win_order[o1],win_order[o2]);
}

WINDOW* &DBGBlock::get_win_ref(int idx) {
    switch (idx) {
        case WINI_REG:  return win_reg;
        case WINI_DATA: return win_data;
        case WINI_CODE: return win_code;
        case WINI_VAR:  return win_var;
        case WINI_OUT:  return win_out;
    }

    throw domain_error("get_win_ref");
}

WINDOW* DBGBlock::get_win(int idx) {
    return get_win_ref(idx);
}

WINDOW *DBGBlock::get_active_win(void) {
    return get_win((int)active_win);
}

int DBGBlock::win_find_order(int wnd) {
    for (unsigned int i=0;i < DBGBlock::WINI_MAX_INDEX;i++) {
        if (dbg.win_order[i] == wnd)
            return (int)i;
    }

    return -1;
}

int DBGBlock::win_prev_by_order(int order) {
    int limit = DBGBlock::WINI_MAX_INDEX;

    do {
        if (--order < 0)
            order = DBGBlock::WINI_MAX_INDEX - 1;

        if (--limit <= 0)
            break;
    } while (get_win(win_order[order]) == NULL);

    return order;
}

int DBGBlock::win_next_by_order(int order) {
    int limit = DBGBlock::WINI_MAX_INDEX;

    do {
        if (++order >= DBGBlock::WINI_MAX_INDEX)
            order = 0;

        if (--limit <= 0)
            break;
    } while (get_win(win_order[order]) == NULL);

    return order;
}

void DBGUI_DrawBlankOutputLine(int y) {
    if (dbg.win_out == NULL) return;

    wattrset(dbg.win_out,COLOR_PAIR(PAIR_GREEN_BLACK));
    if (logBuffHasDiscarded)
        mvwprintw(dbg.win_out, y, 0, "<LOG BUFFER ENDS, OLDER CONTENT DISCARDED BEYOND THIS POINT>");
    else
        mvwprintw(dbg.win_out, y, 0, "<END OF LOG>");
//    wclrtoeol(dbg.win_out);
}

void DBGUI_DrawDebugOutputLine(int y,std::string line) {
	if (dbg.win_out == NULL) return;

	int maxy, maxx; getmaxyx(dbg.win_out,maxy,maxx);
    bool ellipsisEnd = false;

    (void)maxy;//UNUSED

    /* cut the line short if it's too long for the terminal window */
    if (line.length() > (size_t)maxx) {
        line = line.substr(0,(size_t)(maxx - 3));
        ellipsisEnd = true;
    }

    /* Const cast is needed for pdcurses which has no const char in mvwprintw (bug maybe) */
    wattrset(dbg.win_out,0);
    mvwprintw(dbg.win_out, y, 0, const_cast<char*>(line.c_str()));

    if (ellipsisEnd) {
        wattrset(dbg.win_out,COLOR_PAIR(PAIR_GREEN_BLACK));
        mvwprintw(dbg.win_out, y, maxx-3,  "...");
    }

//    wclrtoeol(dbg.win_out);
}

void DEBUG_LimitTopPos(void) {
	if (dbg.win_out != NULL) {
        int w,h;

        getmaxyx(dbg.win_out,h,w);

        auto i = logBuff.begin();
        for (int y=0;i != logBuff.end() && y < (h-1);y++) {
            if (i == logBuffPos) {
                i++;
                logBuffPos = i;
            }
            else {
                i++;
            }
        }
    }
}

void DEBUG_RefreshPage(char scroll) {
	if (dbg.win_out == NULL) return;

	while (scroll < 0 && logBuffPos!=logBuff.begin()) {
        --logBuffPos;
        scroll++;
    }
	while (scroll > 0 && logBuffPos!=logBuff.end()) {
        ++logBuffPos;
        scroll--;
    }

    DEBUG_LimitTopPos();

	list<string>::iterator i = logBuffPos;
	int maxy, maxx; getmaxyx(dbg.win_out,maxy,maxx);
	int rem_lines = maxy;
	if(rem_lines <= 0) return;

	wclear(dbg.win_out);

    /* NTS: Often, i == logBuff.end() unless the user scrolled up in the interface.
     *
     *      So the original intent of this code is that the iterator is one entry PAST
     *      the line to begin displaying at the bottom just as end() is a sentinel
     *      value just past the last element of the list. So if i == logBuff.begin()
     *      then there's nothing to display.
     *
     *      Note that the iterator defines what is drawn on the bottom-most line,
     *      and iteration is done backwards until we've drawn the line at i == logBuff.begin()
     *      or until we've drawn something at line 0 of the subwin.
     *
     *      rem_lines starts out as the number of lines in the subwin. */
    if (i != logBuff.begin()) {
        --i;

        wattrset(dbg.win_out,0);
        while (rem_lines > 0) {
            rem_lines--;

            DBGUI_DrawDebugOutputLine(rem_lines,*i);

            if (i != logBuff.begin())
                --i;
            else
                break;
        }

        /* show that the lines above are beyond the end of the log */
        while (rem_lines > 0) {
            rem_lines--;
            DBGUI_DrawBlankOutputLine(rem_lines);
        }
    }

	wrefresh(dbg.win_out);
}

void DEBUG_ScrollHomeOutput(void) {
    logBuffPos = logBuff.begin();
    DEBUG_RefreshPage(0);
}

void DEBUG_ScrollToEndOutput(void) {
    logBuffPos = logBuff.end();
    DEBUG_RefreshPage(0);
}

static void Draw_RegisterLayout(void) {
	if (dbg.win_main == NULL)
		return;

	mvwaddstr(dbg.win_reg,0,0,"EAX=");
	mvwaddstr(dbg.win_reg,1,0,"EBX=");
	mvwaddstr(dbg.win_reg,2,0,"ECX=");
	mvwaddstr(dbg.win_reg,3,0,"EDX=");

	mvwaddstr(dbg.win_reg,0,14,"ESI=");
	mvwaddstr(dbg.win_reg,1,14,"EDI=");
	mvwaddstr(dbg.win_reg,2,14,"EBP=");
	mvwaddstr(dbg.win_reg,3,14,"ESP=");

	mvwaddstr(dbg.win_reg,0,28,"DS=");
	mvwaddstr(dbg.win_reg,0,38,"ES=");
	mvwaddstr(dbg.win_reg,0,48,"FS=");
	mvwaddstr(dbg.win_reg,0,58,"GS=");
	mvwaddstr(dbg.win_reg,0,68,"SS=");

	mvwaddstr(dbg.win_reg,1,28,"CS=");
	mvwaddstr(dbg.win_reg,1,38,"EIP=");

	mvwaddstr(dbg.win_reg,2,75,"CPL");
	mvwaddstr(dbg.win_reg,2,68,"IOPL");

	mvwaddstr(dbg.win_reg,4,0,"ST0=");
	mvwaddstr(dbg.win_reg,5,0,"ST4=");

	mvwaddstr(dbg.win_reg,4,14,"ST1=");
	mvwaddstr(dbg.win_reg,5,14,"ST5=");

	mvwaddstr(dbg.win_reg,4,28,"ST2=");
	mvwaddstr(dbg.win_reg,5,28,"ST6=");

	mvwaddstr(dbg.win_reg,4,42,"ST3=");
	mvwaddstr(dbg.win_reg,5,42,"ST7=");

	mvwaddstr(dbg.win_reg,1,52,"C  Z  S  O  A  P  D  I  T ");
}

static void DrawSubWinBox(WINDOW *wnd,const char *title) {
    bool active = false;
    int x,y;
    int w,h;

    if (wnd == NULL) return;

    const WINDOW *active_win = dbg.get_active_win();

    if (wnd == active_win)
        active = true;

    getbegyx(wnd,y,x);
    getmaxyx(wnd,h,w);

    (void)h;//UNUSED

	if (has_colors()) {
        if (active)
    		attrset(COLOR_PAIR(PAIR_BLACK_BLUE));
        else
    		attrset(COLOR_PAIR(PAIR_WHITE_BLUE));
    }

    mvhline(y-1,x,ACS_HLINE,w);
    if (title != NULL) mvaddstr(y-1,x+4,title);
}

void DrawBars(void) {
	if (dbg.win_main == NULL)
		return;

    for (unsigned int wnd=0;wnd < DBGBlock::WINI_MAX_INDEX;wnd++) {
        WINDOW* &ref = dbg.get_win_ref((int)wnd);

        if (ref != NULL) DrawSubWinBox(ref,dbg.get_wintitle((int)wnd));
    }

	attrset(0);
}

static void DestroySubWindows(void) {
    for (unsigned int wnd=0;wnd < DBGBlock::WINI_MAX_INDEX;wnd++) {
        WINDOW* &ref = dbg.get_win_ref((int)wnd);

        if (ref != NULL) {
            delwin(ref);
            ref = NULL;
        }
    }
}

void DEBUG_GUI_DestroySubWindows(void) {
    DestroySubWindows();
}

static void MakeSubWindows(void) {
	/* The Std output win should go at the bottom */
	/* Make all the subwindows */
	int win_main_maxy, win_main_maxx; getmaxyx(dbg.win_main,win_main_maxy,win_main_maxx);
    unsigned int yheight[DBGBlock::WINI_MAX_INDEX];
    unsigned int yofs[DBGBlock::WINI_MAX_INDEX];
    int win_limit = win_main_maxy - 1;
    int expand_wndi = -1;
	int outy=0,height;

    /* main window needs to clear itself */
    werase(dbg.win_main);

    /* arrange windows */
    for (unsigned int wndi=0;wndi < DBGBlock::WINI_MAX_INDEX;wndi++) {
        unsigned int wnd = dbg.win_order[wndi];

        /* window must have been freed, must have height greater than 1 (more than just titlebar)
         * must be visible and there must be room for the window on the screen, where the last
         * row is reserved for the input bar */
        if (dbg.win_height[wnd] > 1 &&
            dbg.win_vis[wnd] && (outy+1) < win_limit) {
            outy++; // header
            height=(int)dbg.win_height[wnd] - 1;
            if ((outy+height) > win_limit) height = win_limit - outy;
            assert(height > 0);
            yofs[wndi]=(unsigned int)outy;
            yheight[wndi]=(unsigned int)height;
            outy+=height;

            if (wnd == DBGBlock::WINI_OUT)
                expand_wndi = (int)wndi;
        }
        else {
            yofs[wndi]=0;
            yheight[wndi]=0;
        }
    }

    /* last window expands if output not there */
    if (expand_wndi < 0) {
        for (int wndi=DBGBlock::WINI_MAX_INDEX-1;wndi >= 0;wndi--) {
            if (yheight[wndi] != 0) {
                expand_wndi = wndi;
                break;
            }
        }
    }

    /* we give one window the power to expand to help fill the screen */
    if (outy < win_limit) {
        int expand_by = win_limit - outy;

        if (expand_wndi >= 0 && expand_wndi < DBGBlock::WINI_MAX_INDEX) {
            unsigned int wndi = (unsigned int)expand_wndi;

            /* add height to the window */
            yheight[wndi] += (unsigned int)expand_by;
            outy += (int)expand_by;
            wndi++;

            /* move the others down */
            for (;wndi < DBGBlock::WINI_MAX_INDEX;wndi++)
                yofs[wndi] += (unsigned int)expand_by;
        }
    }

    for (unsigned int wndi=0;wndi < DBGBlock::WINI_MAX_INDEX;wndi++) {
        if (yheight[wndi] != 0) {
            unsigned int wnd = dbg.win_order[wndi];
            WINDOW* &ref = dbg.get_win_ref((int)wnd);
            assert(ref == NULL);
            ref = subwin(dbg.win_main,(int)yheight[wndi],win_main_maxx,(int)yofs[wndi],0);
        }
    }

    if (outy < win_main_maxy) {
        // no header
        height = 1;
        outy = win_main_maxy - height;
        dbg.win_inp=subwin(dbg.win_main,height,win_main_maxx,outy,0);
        outy += height;
    }

	DrawBars();
	Draw_RegisterLayout();
	refresh();
}

void DEBUG_GUI_Rebuild(void) {
    DestroySubWindows();
    MakeSubWindows();
    DEBUG_RefreshPage(0);
}

void DEBUG_GUI_OnResize(void) {
    DEBUG_GUI_Rebuild();
}

static void MakePairs(void) {
	init_pair(PAIR_BLACK_BLUE, COLOR_BLACK, COLOR_CYAN);
	init_pair(PAIR_BYELLOW_BLACK, COLOR_YELLOW /*| FOREGROUND_INTENSITY */, COLOR_BLACK);
	init_pair(PAIR_GREEN_BLACK, COLOR_GREEN /*| FOREGROUND_INTENSITY */, COLOR_BLACK);
	init_pair(PAIR_BLACK_GREY, COLOR_BLACK /*| FOREGROUND_INTENSITY */, COLOR_WHITE);
	init_pair(PAIR_GREY_RED, COLOR_WHITE/*| FOREGROUND_INTENSITY */, COLOR_RED);
    init_pair(PAIR_BLACK_GREEN, COLOR_BLACK, COLOR_GREEN);
	init_pair(PAIR_WHITE_BLUE, COLOR_WHITE/* | FOREGROUND_INTENSITY*/, COLOR_BLUE);
}

void DBGUI_NextWindow(void) {
    dbg.next_window();
	DrawBars();
	DEBUG_DrawScreen();
}

void DBGUI_NextWindowIfActiveHidden(void) {
    if (dbg.get_active_win() == NULL)
        DBGUI_NextWindow();
}

bool DEBUG_IsDebuggerConsoleVisible(void) {
	return (dbg.win_main != NULL);
}

void DEBUG_FlushInput(void) {
    if (dbg.win_main != NULL) {
        while (getch() >= 0); /* remember nodelay() is called to make getch() non-blocking */
    }
}

void DBGUI_StartUp(void) {
	mainMenu.get_item("show_console").check(true).enable(false).refresh_item(mainMenu);

	LOG(LOG_MISC,LOG_DEBUG)("DEBUG GUI startup");
	/* Start the main window */
	dbg.win_main=initscr();

#ifdef WIN32
    /* Tell Windows 10 we DONT want a thin tall console window that fills the screen top to bottom.
       It's a nuisance especially when the user attempts to move the windwo up to the top to see
       the status, only for Windows to auto-maximize. 30 lines is enough, thanks. */
    {
        if (dbg.win_main) {
            int win_main_maxy, win_main_maxx; getmaxyx(dbg.win_main, win_main_maxy, win_main_maxx);
            if (win_main_maxx > 100) win_main_maxy = 100;
            if (win_main_maxy > 40) win_main_maxy = 40;
            resize_term(win_main_maxy, win_main_maxx);
        }
    }
#endif

	cbreak();       /* take input chars one at a time, no wait for \n */
	noecho();       /* don't echo input */
	scrollok(stdscr,false);
	nodelay(dbg.win_main,true);
	keypad(dbg.win_main,true);
	#ifndef WIN32
	touchwin(dbg.win_main);
	#endif
	old_cursor_state = curs_set(0);
	start_color();
	cycle_count=0;
	MakePairs();
	MakeSubWindows();

    /* make sure the output window is synced up */
    logBuffPos = logBuff.end();
    DEBUG_RefreshPage(0);
}

#endif

int debugPageCounter = 0;
int debugPageStopAt = 0;

bool DEBUG_IsPagingOutput(void) {
    return debugPageStopAt > 0;
}

void DEBUG_DrawInput(void);

void DEBUG_BeginPagedContent(void) {
#if C_DEBUG
	int maxy, maxx; getmaxyx(dbg.win_out,maxy,maxx);

    debugPageCounter = 0;
    debugPageStopAt = maxy;
#endif
}

void DEBUG_EndPagedContent(void) {
#if C_DEBUG
    debugPageCounter = 0;
    debugPageStopAt = 0;
    DEBUG_DrawInput();
#endif
}

extern bool gfx_in_mapper;

bool in_debug_showmsg = false;

bool IsDebuggerActive(void);

void DEBUG_ShowMsg(char const* format,...) {
	bool stderrlog = false;
	char buf[512];
	va_list msg;
	size_t len;

    in_debug_showmsg = true;

    // in case of runaway error from the CPU core, user responsiveness can be helpful
    CPU_CycleLeft += CPU_Cycles;
    CPU_Cycles = 0;

    if (!gfx_in_mapper && !in_debug_showmsg) {
        void GFX_Events();
        GFX_Events();
    }

    va_start(msg,format);
	len = (size_t)vsnprintf(buf,sizeof(buf)-2u,format,msg); /* <- NTS: Did you know sprintf/vsnprintf returns number of chars written? */
	va_end(msg);

    /* remove newlines if present */
    while (len > 0 && buf[len-1] == '\n') buf[--len] = 0;

#if C_DEBUG
	if (dbg.win_out != NULL)
		stderrlog = false;
    else
        stderrlog = true;
#else
	if (do_LOG_stderr || debuglog == NULL)
		stderrlog = true;
#endif

	if (debuglog != NULL) {
		fprintf(debuglog,"%s\n",buf);
		fflush(debuglog);
	}
	if (stderrlog) {
#if C_EMSCRIPTEN
        /* Emscripten routes stderr to the browser console.error() function, and
         * stdout to a console window below ours on the browser page. We want the
         * user to see our blather, so print to stdout */
		fprintf(stdout,"LOG: %s\n",buf);
		fflush(stdout);
#else
		fprintf(stderr,"LOG: %s\n",buf);
		fflush(stderr);
#endif
	}

#if C_DEBUG
	if (logBuffPos!=logBuff.end()) {
		logBuffPos=logBuff.end();
		DEBUG_RefreshPage(0);
	}
	logBuff.push_back(buf);
	if (logBuff.size() > MAX_LOG_BUFFER) {
        logBuffHasDiscarded = true;
        if (logBuffPos == logBuff.begin()) ++logBuffPos; /* keep the iterator valid */
		logBuff.pop_front();
    }

	logBuffPos = logBuff.end();

	if (dbg.win_out != NULL) {
        if (logBuffSuppressConsole) {
            logBuffSuppressConsoleNeedUpdate = true;
        }
        else {
            int maxy, maxx; getmaxyx(dbg.win_out,maxy,maxx);

            scrollok(dbg.win_out,TRUE);
            scroll(dbg.win_out);
            scrollok(dbg.win_out,FALSE);

            DBGUI_DrawDebugOutputLine(maxy-1,buf);

            wrefresh(dbg.win_out);
        }
	}

    if (IsDebuggerActive() && debugPageStopAt > 0) {
        if (++debugPageCounter >= debugPageStopAt) {
            debugPageCounter = 0;
            DEBUG_RefreshPage(0);
            DEBUG_DrawInput();

            /* pause, wait for input */
            do {
#if defined(WIN32)
                int key;

                if (kbhit())
                    key = getch();
                else
                    key = -1;
#else
                int key = getch();
#endif
                if (key > 0) {
                    if (key == ' ' || key == 0x0A) {
                        /* continue */
                        break;
                    }
                    else if (key == 0x27/*ESC*/ || key == 0x7F/*DEL*/ || key == 0x08/*BKSP*/ ||
                             key == 'q' || key == 'Q') {
                        /* user wants to stop paging */
                        debugPageStopAt = 0;
                        break;
                    }
                }

#if defined(WIN32)
                /* help inspire confidence in users and in Windows by keeping the event loop going.
                   This is to prevent Windows from graying out the main window during this loop
                   as "application not responding" */
                SDL_Event ev;

                if (SDL_PollEvent(&ev)) {
                    /* TODO */
                }
#endif
            } while (1);
        }
    }
#endif

    in_debug_showmsg = false;
}

/* callback function when DOSBox-X exits */
void LOG::Exit() {
	if (debuglog != NULL) {
		fprintf(debuglog,"--END OF LOG--\n");
		fclose(debuglog);
		debuglog = NULL;
	}
}

void Null_Init(Section *sec);

void LOG::operator() (char const* format, ...){
	const char *s_severity = "";
	char buf[512];
	va_list msg;

	switch (d_severity) {
		case LOG_DEBUG:	s_severity = " DEBUG"; break;
		case LOG_NORMAL:s_severity = "      "; break;
		case LOG_WARN:  s_severity = " WARN "; break;
		case LOG_ERROR: s_severity = " ERROR"; break;
		case LOG_FATAL: s_severity = " FATAL"; break;
		default: break;
	}

	va_start(msg,format);
	vsnprintf(buf,sizeof(buf)-1,format,msg);
	va_end(msg);

	if (d_type>=LOG_MAX) return;
	if (d_severity < loggrp[d_type].min_severity) return;
	DEBUG_ShowMsg("%10u%s %s:%s\n",static_cast<uint32_t>(cycle_count),s_severity,loggrp[d_type].front,buf);
}

void LOG::ParseEnableSetting(_LogGroup &group,const char *setting) {
	if (!strcmp(setting,"true") || !strcmp(setting,"1") || !strcmp(setting,"normal"))
		group.min_severity = LOG_NORMAL; /* original code's handling is equivalent to our "normal" setting */
	else if (!strcmp(setting,"false") || !strcmp(setting,"0") || !strcmp(setting,""))
		group.min_severity = LOG_ERROR; /* original code's handling is equivalent to our "error" setting */
	else if (!strcmp(setting,"debug"))
		group.min_severity = LOG_DEBUG;
	else if (!strcmp(setting,"warn"))
		group.min_severity = LOG_WARN;
	else if (!strcmp(setting,"error"))
		group.min_severity = LOG_ERROR;
	else if (!strcmp(setting,"fatal"))
		group.min_severity = LOG_FATAL;
	else if (!strcmp(setting,"never"))
		group.min_severity = LOG_NEVER;
	else
		group.min_severity = LOG_NORMAL;
}

void ResolvePath(std::string& in);
void LOG::Init() {
	char buf[64];

	assert(control != NULL);

	/* do not init twice */
	if (has_LOG_Init) return;
	has_LOG_Init = true;

	/* announce */
	LOG_MSG("Logging init: beginning logging proper. This is the end of the early init logging");

	/* get the [log] section */
	const Section_prop *sect = static_cast<Section_prop *>(control->GetSection("log"));
	assert(sect != NULL);

	/* do we write to a logfile, or not? */
	std::string logfile = sect->Get_string("logfile");
	if (logfile.size()) {
		ResolvePath(logfile);
		if ((debuglog=fopen(logfile.c_str(),"wt+")) != NULL) {
			LOG_MSG("Logging: opened logfile '%s' successfully. All further logging will go to this file.",logfile.c_str());
			setbuf(debuglog,NULL);
		}
		else {
			LOG_MSG("Logging: failed to open logfile '%s'. All further logging will be discarded. Error: %s",logfile.c_str(),strerror(errno));
		}
	}
	else {
		LOG_MSG("Logging: No logfile was given. All further logging will be discarded.");
		debuglog=0;
	}

    log_int21 = sect->Get_bool("int21") || control->opt_logint21;
    log_fileio = sect->Get_bool("fileio") || control->opt_logfileio;

	/* end of early init logging */
	do_LOG_stderr = false;

	/* read settings for each log category, unless the -debug option was given,
	 * in which case everything is set to debug level */
	for (Bitu i = LOG_ALL + 1;i < LOG_MAX;i++) { //Skip LOG_ALL, it is always enabled
		safe_strncpy(buf,loggrp[i].front,sizeof(buf));
		lowcase(buf);

		if (control->opt_debug)
			ParseEnableSetting(/*&*/loggrp[i],"debug");
		else
			ParseEnableSetting(/*&*/loggrp[i],sect->Get_string(buf));
	}

	LOG(LOG_MISC,LOG_DEBUG)("Logging init complete");
}

void LOG::EarlyInit(void) {
	assert(control != NULL);

	/* do not init twice */
	if (has_LOG_EarlyInit) return;
	has_LOG_EarlyInit = true;

	/* Setup logging groups */
	loggrp[LOG_ALL].front="ALL";
	loggrp[LOG_VGA].front="VGA";
	loggrp[LOG_VGAGFX].front="VGAGFX";
	loggrp[LOG_VGAMISC].front="VGAMISC";
	loggrp[LOG_INT10].front="INT10";
	loggrp[LOG_SB].front="SBLASTER";
	loggrp[LOG_DMACONTROL].front="DMA_CONTROL";
	
	loggrp[LOG_FPU].front="FPU";
	loggrp[LOG_CPU].front="CPU";
	loggrp[LOG_PAGING].front="PAGING";

	loggrp[LOG_FCB].front="FCB";
	loggrp[LOG_FILES].front="FILES";
	loggrp[LOG_IOCTL].front="IOCTL";
	loggrp[LOG_EXEC].front="EXEC";
	loggrp[LOG_DOSMISC].front="DOSMISC";

	loggrp[LOG_PIT].front="PIT";
	loggrp[LOG_KEYBOARD].front="KEYBOARD";
	loggrp[LOG_PIC].front="PIC";

	loggrp[LOG_MOUSE].front="MOUSE";
	loggrp[LOG_BIOS].front="BIOS";
	loggrp[LOG_GUI].front="GUI";
	loggrp[LOG_MISC].front="MISC";

	loggrp[LOG_IO].front="IO";
	loggrp[LOG_PCI].front="PCI";
	
	loggrp[LOG_VOODOO].front="SST";

	do_LOG_stderr = control->opt_earlydebug;

	for (Bitu i=1;i<LOG_MAX;i++) {
		if (control->opt_earlydebug)
			ParseEnableSetting(/*&*/loggrp[i],"debug");
		else
			ParseEnableSetting(/*&*/loggrp[i],"warn");
	}

	LOG_MSG("Early LOG Init complete");
}

void LOG::SetupConfigSection(void) {
	const char *log_values[] = {
		/* compatibility with existing dosbox.conf files */
		"true", "false",

		/* log levels */
		"debug",
		"normal",
		"warn",
		"error",
		"fatal",
		"never",		/* <- this means NEVER EVER log anything */

		0};

	/* Register the log section */
	Section_prop * sect=control->AddSection_prop("log",Null_Init);
	Prop_string* Pstring = sect->Add_string("logfile",Property::Changeable::Always,"");
	Pstring->Set_help("file where the log messages will be saved to");
	Pstring->SetBasic(true);
	char buf[64];
	for (Bitu i = LOG_ALL + 1;i < LOG_MAX;i++) {
		safe_strncpy(buf,loggrp[i].front, sizeof(buf));
		lowcase(buf);

		Pstring = sect->Add_string(buf,Property::Changeable::Always,"false");
		Pstring->Set_values(log_values);
		Pstring->Set_help("Enable/Disable logging of this type.");
    }

    Prop_bool* Pbool;

    Pbool = sect->Add_bool("int21",Property::Changeable::Always,false);
    Pbool->Set_help("Log all INT 21h calls");

    Pbool = sect->Add_bool("fileio",Property::Changeable::Always,false);
    Pbool->Set_help("Log file I/O through INT 21h");
}

