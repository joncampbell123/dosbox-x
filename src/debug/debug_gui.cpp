/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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


#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "dosbox.h"
#include "logging.h"
#include "support.h"
#include "control.h"
#include "regs.h"
#include "debug.h"
#include "debug_inc.h"

#include <stdexcept>
#include <exception>

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

static bool has_LOG_Init = false;
static bool has_LOG_EarlyInit = false;
static bool do_LOG_stderr = false;
static bool logBuffHasDiscarded = false;

_LogGroup loggrp[LOG_MAX]={{"",LOG_NORMAL},{0,LOG_NORMAL}};
FILE* debuglog = NULL;

const unsigned int dbg_def_win_height[DBGBlock::WINI_MAX_INDEX] = {
    5,          /* WINI_REG */
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

#if C_DEBUG
#include <curses.h>

#include <list>
#include <string>
using namespace std;

#define MAX_LOG_BUFFER 4000
static list<string> logBuff;
static list<string>::iterator logBuffPos = logBuff.end();

extern int old_cursor_state;

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
            return i;
    }

    return -1;
}

void DBGBlock::next_window(void) {
    int limit = DBGBlock::WINI_MAX_INDEX;
    int order = win_find_order(active_win);

    if (order < 0) order = 0;

    do {
        if (++order >= DBGBlock::WINI_MAX_INDEX)
            order = 0;

        active_win = win_order[order];
        if (--limit <= 0)
            break;
    } while (get_active_win() == NULL);
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
    return get_win(active_win);
}

int DBGBlock::win_find_order(int wnd) {
    for (unsigned int i=0;i < DBGBlock::WINI_MAX_INDEX;i++) {
        if (dbg.win_order[i] == wnd)
            return i;
    }

    return -1;
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

    /* cut the line short if it's too long for the terminal window */
    if (line.length() > maxx) {
        line = line.substr(0,maxx-3);
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
        logBuffPos--;
        scroll++;
    }
	while (scroll > 0 && logBuffPos!=logBuff.end()) {
        logBuffPos++;
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
        i--;

        wattrset(dbg.win_out,0);
        while (rem_lines > 0) {
            rem_lines--;

            DBGUI_DrawDebugOutputLine(rem_lines,*i);

            if (i != logBuff.begin())
                i--;
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

	mvwaddstr(dbg.win_reg,1,52,"C  Z  S  O  A  P  D  I  T ");
}

static void DrawSubWinBox(WINDOW *wnd,const char *title) {
    bool active = false;
    int x,y;
    int w,h;

    if (wnd == NULL) return;

    WINDOW *active_win = dbg.get_active_win();

    if (wnd == active_win)
        active = true;

    getbegyx(wnd,y,x);
    getmaxyx(wnd,h,w);

	if (has_colors()) {
        if (active)
    		attrset(COLOR_PAIR(PAIR_BLACK_BLUE));
        else
    		attrset(COLOR_PAIR(PAIR_WHITE_BLUE));
    }

    mvhline(y-1,x,ACS_HLINE,w);
    if (title != NULL) mvaddstr(y-1,x+4,title);
}

static void DrawBars(void) {
	if (dbg.win_main == NULL)
		return;

    for (unsigned int wnd=0;wnd < DBGBlock::WINI_MAX_INDEX;wnd++) {
        WINDOW* &ref = dbg.get_win_ref(wnd);

        if (ref != NULL) DrawSubWinBox(ref,dbg.get_wintitle(wnd));
    }

	attrset(0);
}

static void DestroySubWindows(void) {
    for (unsigned int wnd=0;wnd < DBGBlock::WINI_MAX_INDEX;wnd++) {
        WINDOW* &ref = dbg.get_win_ref(wnd);

        if (ref != NULL) {
            if (ref) delwin(ref);
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
            height=dbg.win_height[wnd]-1;
            if ((outy+height) > win_limit) height = win_limit - outy;
            assert(height > 0);
            yofs[wndi]=outy;
            yheight[wndi]=height;
            outy+=height;

            if (wnd == DBGBlock::WINI_OUT)
                expand_wndi = wndi;
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
            unsigned int wndi = expand_wndi;

            /* add height to the window */
            yheight[wndi] += expand_by;
            outy += wndi;
            wndi++;

            /* move the others down */
            for (;wndi < DBGBlock::WINI_MAX_INDEX;wndi++)
                yofs[wndi] += expand_by;
        }
    }

    for (unsigned int wndi=0;wndi < DBGBlock::WINI_MAX_INDEX;wndi++) {
        if (yheight[wndi] != 0) {
            unsigned int wnd = dbg.win_order[wndi];
            WINDOW* &ref = dbg.get_win_ref(wnd);
            assert(ref == NULL);
            ref=subwin(dbg.win_main,yheight[wndi],win_main_maxx,yofs[wndi],0);
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

void DBGUI_StartUp(void) {
	LOG(LOG_MISC,LOG_DEBUG)("DEBUG GUI startup");
	/* Start the main window */
	dbg.win_main=initscr();
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

void DEBUG_ShowMsg(char const* format,...) {
	bool stderrlog = false;
	char buf[512];
	va_list msg;
	size_t len;

	va_start(msg,format);
	len = vsnprintf(buf,sizeof(buf)-2,format,msg); /* <- NTS: Did you know sprintf/vsnprintf returns number of chars written? */
	va_end(msg);

    /* remove newlines if present */
    while (len > 0 && buf[len-1] == '\n') buf[--len] = 0;

	if (do_LOG_stderr || debuglog == NULL)
		stderrlog = true;

#if C_DEBUG
	if (dbg.win_out != NULL)
		stderrlog = false;
    else
        stderrlog = true;
#endif

	if (debuglog != NULL) {
		fprintf(debuglog,"%s\n",buf);
		fflush(debuglog);
	}
	if (stderrlog) {
		fprintf(stderr,"LOG: %s\n",buf);
		fflush(stderr);
	}

#if C_DEBUG
	if (logBuffPos!=logBuff.end()) {
		logBuffPos=logBuff.end();
		DEBUG_RefreshPage(0);
	}
	logBuff.push_back(buf);
	if (logBuff.size() > MAX_LOG_BUFFER) {
        logBuffHasDiscarded = true;
		logBuff.pop_front();
    }

	logBuffPos = logBuff.end();

	if (dbg.win_out != NULL) {
        int maxy, maxx; getmaxyx(dbg.win_out,maxy,maxx);

    	scrollok(dbg.win_out,TRUE);
        scroll(dbg.win_out);
    	scrollok(dbg.win_out,FALSE);

        DBGUI_DrawDebugOutputLine(maxy-1,buf);

		wrefresh(dbg.win_out);
	}
#endif
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
	};

	va_start(msg,format);
	vsnprintf(buf,sizeof(buf)-1,format,msg);
	va_end(msg);

	if (d_type>=LOG_MAX) return;
	if (d_severity < loggrp[d_type].min_severity) return;
	DEBUG_ShowMsg("%10u%s %s:%s\n",static_cast<Bit32u>(cycle_count),s_severity,loggrp[d_type].front,buf);
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

void LOG::Init() {
	char buf[1024];

	assert(control != NULL);

	/* do not init twice */
	if (has_LOG_Init) return;
	has_LOG_Init = true;

	/* announce */
	LOG_MSG("Logging init: beginning logging proper. This is the end of the early init logging");

	/* get the [log] section */
	Section_prop *sect = static_cast<Section_prop *>(control->GetSection("log"));
	assert(sect != NULL);

	/* do we write to a logfile, or not? */
	const char *blah = sect->Get_string("logfile");
	if (blah != NULL && blah[0] != 0) {
		if ((debuglog=fopen(blah,"wt+")) != NULL) {
			LOG_MSG("Logging: opened logfile '%s' successfully. All further logging will go to that file.",blah);
			setbuf(debuglog,NULL);
		}
		else {
			LOG_MSG("Logging: failed to open logfile '%s'. All further logging will be discarded. Error: %s",blah,strerror(errno));
		}
	}
	else {
		LOG_MSG("Logging: No logfile was given. All further logging will be discarded.");
		debuglog=0;
	}

	/* end of early init logging */
	do_LOG_stderr = false;

	/* read settings for each log category, unless the -debug option was given,
	 * in which case everything is set to debug level */
	for (Bitu i=1;i<LOG_MAX;i++) {
		strcpy(buf,loggrp[i].front);
		buf[strlen(buf)]=0;
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
	char buf[1024];
	for (Bitu i=1;i<LOG_MAX;i++) {
		strcpy(buf,loggrp[i].front);
		lowcase(buf);

		Pstring = sect->Add_string(buf,Property::Changeable::Always,"false");
		Pstring->Set_values(log_values);
		Pstring->Set_help("Enable/Disable logging of this type.");
	}
}

