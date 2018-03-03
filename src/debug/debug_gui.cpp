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

static bool has_LOG_Init = false;
static bool has_LOG_EarlyInit = false;
static bool do_LOG_stderr = false;

_LogGroup loggrp[LOG_MAX]={{"",LOG_NORMAL},{0,LOG_NORMAL}};
FILE* debuglog = NULL;

#if C_DEBUG
#include <curses.h>

#include <list>
#include <string>
using namespace std;

#define MAX_LOG_BUFFER 500
static list<string> logBuff;
static list<string>::iterator logBuffPos = logBuff.end();

extern int old_cursor_state;

void DEBUG_RefreshPage(char scroll) {
	if (dbg.win_out == NULL) return;

	if (scroll==-1 && logBuffPos!=logBuff.begin()) logBuffPos--;
	else if (scroll==1 && logBuffPos!=logBuff.end()) logBuffPos++;

	list<string>::iterator i = logBuffPos;
	int maxy, maxx; getmaxyx(dbg.win_out,maxy,maxx);
	int rem_lines = maxy - 1;
	if(rem_lines == -1) return;

	wclear(dbg.win_out);
	while (rem_lines > 0 && i!=logBuff.begin()) {
		--i;
		/* Const cast is needed for pdcurses which has no const char in mvwprintw (bug maybe) */
		mvwprintw(dbg.win_out,rem_lines, 0, const_cast<char*>((*i).c_str()));
		rem_lines--;
	}
	wrefresh(dbg.win_out);
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


static void DrawBars(void) {
	if (dbg.win_main == NULL)
		return;

	if (has_colors()) {
		attrset(COLOR_PAIR(PAIR_BLACK_BLUE));
	}
	/* Show the Register bar */
	mvaddstr(1-1,0, "---(Register Overview                   )---");
	/* Show the Data Overview bar perhaps with more special stuff in the end */
	mvaddstr(6-1,0,"---(Data Overview   Scroll: page up/down)---");
	/* Show the Code Overview perhaps with special stuff in bar too */
	mvaddstr(17-1,0,"---(Code Overview   Scroll: up/down     )---");
	/* Show the Variable Overview bar */
	mvaddstr(29-1,0, "---(Variable Overview                   )---");
	/* Show the Output OverView */
	mvaddstr(34-1,0, "---(Output          Scroll: home/end    )---");
	attrset(0);
	//Match values with below. So we don't need to touch the internal window structures
}

static void DestroySubWindows(void) {
    if (dbg.win_out) delwin(dbg.win_out);
    dbg.win_out = NULL;

    if (dbg.win_var) delwin(dbg.win_var);
    dbg.win_var = NULL;

    if (dbg.win_reg) delwin(dbg.win_reg);
    dbg.win_reg = NULL;

    if (dbg.win_code) delwin(dbg.win_code);
    dbg.win_code = NULL;

    if (dbg.win_data) delwin(dbg.win_data);
    dbg.win_data = NULL;
}

void DEBUG_GUI_DestroySubWindows(void) {
    DestroySubWindows();
}

static void MakeSubWindows(void) {
	/* The Std output win should go at the bottom */
	/* Make all the subwindows */
	int win_main_maxy, win_main_maxx; getmaxyx(dbg.win_main,win_main_maxy,win_main_maxx);
	int outy=1; //Match values with above

    LOG_MSG("DEBUG: MakeSubWindows dim x=%u y=%u",win_main_maxx,win_main_maxy);

	/* The Register window  */
	dbg.win_reg=subwin(dbg.win_main,4,win_main_maxx,outy,0);
	outy+=5; // 6
	/* The Data Window */
	dbg.win_data=subwin(dbg.win_main,10,win_main_maxx,outy,0);
	outy+=11; // 17
	/* The Code Window */
	dbg.win_code=subwin(dbg.win_main,11,win_main_maxx,outy,0);
	outy+=12; // 29
	/* The Variable Window */
	dbg.win_var=subwin(dbg.win_main,4,win_main_maxx,outy,0);
	outy+=5; // 34
	/* The Output Window */	
	dbg.win_out=subwin(dbg.win_main,win_main_maxy-outy,win_main_maxx,outy,0);
	if(!dbg.win_reg ||!dbg.win_data || !dbg.win_code || !dbg.win_var || !dbg.win_out) E_Exit("Setting up windows failed");
//	dbg.input_y=win_main_maxy-1;
	scrollok(dbg.win_out,TRUE);
	DrawBars();
	Draw_RegisterLayout();
	refresh();
}

void DEBUG_GUI_OnResize(void) {
    DestroySubWindows();
    MakeSubWindows();
}

static void MakePairs(void) {
	init_pair(PAIR_BLACK_BLUE, COLOR_BLACK, COLOR_CYAN);
	init_pair(PAIR_BYELLOW_BLACK, COLOR_YELLOW /*| FOREGROUND_INTENSITY */, COLOR_BLACK);
	init_pair(PAIR_GREEN_BLACK, COLOR_GREEN /*| FOREGROUND_INTENSITY */, COLOR_BLACK);
	init_pair(PAIR_BLACK_GREY, COLOR_BLACK /*| FOREGROUND_INTENSITY */, COLOR_WHITE);
	init_pair(PAIR_GREY_RED, COLOR_WHITE/*| FOREGROUND_INTENSITY */, COLOR_RED);
    init_pair(PAIR_BLACK_GREEN, COLOR_BLACK, COLOR_GREEN);
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
	if (logBuff.size() > MAX_LOG_BUFFER)
		logBuff.pop_front();

	logBuffPos = logBuff.end();

	if (dbg.win_out != NULL) {
		wprintw(dbg.win_out,"%s\n",buf);
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

