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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm> //std::copy
#include <iterator>  //std::front_inserter
#include <regex>

#include "logging.h"
#include "shell.h"
#include "timer.h"
#include "bios.h"
#include "control.h"
#include "regs.h"
#include "callback.h"
#include "support.h"
#include "inout.h"
#include "render.h"
#include "../ints/int10.h"
#include "../dos/drives.h"

#ifdef _MSC_VER
# if !defined(C_SDL2)
#  include "process.h"
# endif
# define MIN(a,b) ((a) < (b) ? (a) : (b))
# define MAX(a,b) ((a) > (b) ? (a) : (b))
#else
# define MIN(a,b) std::min(a,b)
# define MAX(a,b) std::max(a,b)
#endif

bool clearline=false, inshell=false, noassoc=false;
int autofixwarn=3;
extern int lfn_filefind_handle;
extern bool ctrlbrk, gbk, rtl, dbcs_sbcs;
extern bool DOS_BreakFlag, DOS_BreakConioFlag;
extern uint16_t cmd_line_seg;
uint8_t prompt_col; // Column position after prompt is displayed
void WriteChar(uint16_t col, uint16_t row, uint8_t page, uint16_t chr, uint8_t attr, bool useattr);

#if defined(USE_TTF)
extern bool ttf_dosv;
#endif
extern std::map<int, int> pc98boxdrawmap;

void DOS_Shell::ShowPrompt(void) {
	char dir[DOS_PATHLENGTH];
	dir[0] = 0; //DOS_GetCurrentDir doesn't always return something. (if drive is messed up)
	DOS_GetCurrentDir(0,dir,uselfn);
	std::string line;
	const char * promptstr = "\0";
    inshell = true;

	if(GetEnvStr("PROMPT",line)) {
		std::string::size_type idx = line.find('=');
		std::string value=line.substr(idx +1 , std::string::npos);
		line = std::string(promptstr) + value;
		promptstr = line.c_str();
	}

	while (*promptstr) {
		if (!strcasecmp(promptstr,"$"))
			WriteOut("\0");
		else if(*promptstr != '$')
			WriteOut("%c",*promptstr);
		else switch (toupper(*++promptstr)) {
			case 'A': WriteOut("&"); break;
			case 'B': WriteOut("|"); break;
			case 'C': WriteOut("("); break;
			case 'D': WriteOut("%02d-%02d-%04d",dos.date.day,dos.date.month,dos.date.year); break;
			case 'E': WriteOut("%c",27);  break;
			case 'F': WriteOut(")");  break;
			case 'G': WriteOut(">"); break;
			case 'H': WriteOut("\b");   break;
			case 'L': WriteOut("<"); break;
			case 'N': WriteOut("%c",DOS_GetDefaultDrive()+'A'); break;
			case 'P': WriteOut("%c:\\",DOS_GetDefaultDrive()+'A'); WriteOut_NoParsing(dir, true); break;
			case 'Q': WriteOut("="); break;
			case 'S': WriteOut(" "); break;
			case 'T': {
				Bitu ticks=(Bitu)(((65536.0 * 100.0)/(double)PIT_TICK_RATE)* mem_readd(BIOS_TIMER));
				reg_dl=(uint8_t)((Bitu)ticks % 100);
				ticks/=100;
				reg_dh=(uint8_t)((Bitu)ticks % 60);
				ticks/=60;
				reg_cl=(uint8_t)((Bitu)ticks % 60);
				ticks/=60;
				reg_ch=(uint8_t)((Bitu)ticks % 24);
				WriteOut("%d:%02d:%02d.%02d",reg_ch,reg_cl,reg_dh,reg_dl);
				break;
			}
#if !defined(OS2) || !defined(C_SDL2)
			case 'V': WriteOut("DOSBox-X version %s. Reported DOS version %d.%d.",VERSION,dos.version.major,dos.version.minor); break;
#else
			case 'V': WriteOut("DOSBox-X version %s. Reported DOS version %d.%d.",PACKAGE_VERSION,dos.version.major,dos.version.minor); break;
#endif
			case '$': WriteOut("$"); break;
			case '_': WriteOut("\n"); break;
			case 'M': break;
			case '+': break;
		}
		promptstr++;
	}
    inshell = false;
    prompt_col = CURSOR_POS_COL(real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE));
}

static void outc(uint8_t c) {
	uint16_t n=1;
	DOS_WriteFile(STDOUT,&c,&n);
}

static void backone() {
	BIOS_NCOLS;
	uint8_t page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
	if (CURSOR_POS_COL(page)>0)
		outc(8);
	else if (CURSOR_POS_ROW(page)>0)
		INT10_SetCursorPos(CURSOR_POS_ROW(page)-1, ncols-1, page);
}

//! \brief Moves the caret to prev row/last column when column is 0 (video mode 0).
void MoveCaretBackwards()
{
	uint8_t col, row;
	const uint8_t page(0);
	INT10_GetCursorPos(&row, &col, page);

	if (col != 0) 
		return;

	uint16_t cols;
	INT10_GetScreenColumns(&cols);
	INT10_SetCursorPos(row - 1, static_cast<uint8_t>(cols), page);
}

bool DOS_Shell::BuildCompletions(char * line, uint16_t str_len) {
    // build new completion list
    // Lines starting with CD/MD/RD will only get directories in the list
    bool dir_only = (strncasecmp(ltrim(line),"CD ",3)==0)||(strncasecmp(ltrim(line),"MD ",3)==0)||(strncasecmp(ltrim(line),"RD ",3)==0)||
            (strncasecmp(ltrim(line),"CHDIR ",6)==0)||(strncasecmp(ltrim(line),"MKDIR ",3)==0)||(strncasecmp(ltrim(line),"RMDIR ",6)==0);
    int q=0, r=0, k=0;

    // get completion mask
    const char *p_completion_start = strrchr(line, ' ');
    while (p_completion_start) {
        q=0;
        char *i;
        for (i=line;i<p_completion_start;i++)
           if (*i=='\"') q++;
        if (q/2*2==q) break;
        *i=0;
        p_completion_start = strrchr(line, ' ');
        *i=' ';
    }
    char c[]={'<','>','|'};
    for (unsigned int j=0; j<sizeof(c); j++) {
        const char *sp = strrchr_dbcs(line, c[j]);
        while (sp) {
            q=0;
            char *i;
            for (i=line;i<sp;i++)
                if (*i=='\"') q++;
            if (q/2*2==q) break;
            *i=0;
            sp = strrchr_dbcs(line, c[j]);
            *i=c[j];
        }
        if (!p_completion_start || p_completion_start<sp)
            p_completion_start = sp;
    }

    if (p_completion_start) {
        p_completion_start ++;
        completion_index = (uint16_t)(str_len - strlen(p_completion_start));
    } else {
        p_completion_start = line;
        completion_index = 0;
    }
    k=completion_index;

    const char *path;
    if ((path = strrchr(line+completion_index,':'))) completion_index = (uint16_t)(path-line+1);
    if ((path = strrchr_dbcs(line+completion_index,'\\'))) completion_index = (uint16_t)(path-line+1);
    if ((path = strrchr(line+completion_index,'/'))) completion_index = (uint16_t)(path-line+1);

    // build the completion list
    char mask[DOS_PATHLENGTH+2] = {0}, smask[DOS_PATHLENGTH] = {0};
    if (p_completion_start && strlen(p_completion_start) + 3 >= DOS_PATHLENGTH) {
        // TODO: This really should be done in the CON driver so that this code can just print ASCII code 7 instead
        if (IS_PC98_ARCH) {
            // TODO: BEEP. I/O PORTS ARE DIFFERENT AS IS THE PIT CLOCK RATE
        }
        else {
            // IBM PC/XT/AT
            IO_Write(0x43,0xb6);
            IO_Write(0x42,1750&0xff);
            IO_Write(0x42,1750>>8);
            IO_Write(0x61,IO_Read(0x61)|0x3);
            for(Bitu i=0; i < 333; i++) CALLBACK_Idle();
            IO_Write(0x61,IO_Read(0x61)&~0x3);
        }
        return false;
    }
    if (p_completion_start) {
        safe_strncpy(mask, p_completion_start,DOS_PATHLENGTH);
        const char* dot_pos = strrchr(mask, '.');
        const char* bs_pos = strrchr_dbcs(mask, '\\');
        const char* fs_pos = strrchr(mask, '/');
        const char* cl_pos = strrchr(mask, ':');
        // not perfect when line already contains wildcards, but works
        if ((dot_pos-bs_pos>0) && (dot_pos-fs_pos>0) && (dot_pos-cl_pos>0))
            strncat(mask, "*",DOS_PATHLENGTH - 1);
        else strncat(mask, "*.*",DOS_PATHLENGTH - 1);
    } else {
        strcpy(mask, "*.*");
    }

    RealPt save_dta=dos.dta();
    dos.dta(dos.tables.tempdta);

    bool res = false;
    if (DOS_GetSFNPath(mask,smask,false)) {
        sprintf(mask,"\"%s\"",smask);
        int fbak=lfn_filefind_handle;
        lfn_filefind_handle=uselfn?LFN_FILEFIND_INTERNAL:LFN_FILEFIND_NONE;
        res = DOS_FindFirst(mask, 0xffff & ~DOS_ATTR_VOLUME);
        lfn_filefind_handle=fbak;
    }
    if (!res) {
        dos.dta(save_dta);
        // TODO: This really should be done in the CON driver so that this code can just print ASCII code 7 instead
        if (IS_PC98_ARCH) {
            // TODO: BEEP. I/O PORTS ARE DIFFERENT AS IS THE PIT CLOCK RATE
        }
        else {
            // IBM PC/XT/AT
            IO_Write(0x43,0xb6);
            IO_Write(0x42,1750&0xff);
            IO_Write(0x42,1750>>8);
            IO_Write(0x61,IO_Read(0x61)|0x3);
            for(Bitu i=0; i < 300; i++) CALLBACK_Idle();
            IO_Write(0x61,IO_Read(0x61)&~0x3);
        }
        return false;
    }

    DOS_DTA dta(dos.dta());
    char name[DOS_NAMELENGTH_ASCII], lname[LFN_NAMELENGTH], qlname[LFN_NAMELENGTH+2];
    uint32_t sz,hsz;uint16_t date;uint16_t time;uint8_t att;

    std::list<std::string> executable;
    q=0;r=0;
    while (*p_completion_start) {
        k++;
        if (*p_completion_start++=='\"') {
            if (k<=completion_index)
                q++;
            else
                r++;
        }
    }
    int fbak=lfn_filefind_handle;
    lfn_filefind_handle=uselfn?LFN_FILEFIND_INTERNAL:LFN_FILEFIND_NONE;
    while (res) {
        dta.GetResult(name,lname,sz,hsz,date,time,att);
        if ((strchr(uselfn?lname:name,' ')!=NULL&&q/2*2==q)||r)
            sprintf(qlname,q/2*2!=q?"%s\"":"\"%s\"",uselfn?lname:name);
        else
            strcpy(qlname,uselfn?lname:name);
        // add result to completion list

        if (strcmp(name, ".") && strcmp(name, "..")) {
            if (dir_only) { //Handle the dir only case different (line starts with cd)
                if(att & DOS_ATTR_DIRECTORY) l_completion.emplace_back(qlname);
            } else {
                const char *ext = strrchr(name, '.'); // file extension
                if ((ext && (strcmp(ext, ".BAT") == 0 || strcmp(ext, ".COM") == 0 || strcmp(ext, ".EXE") == 0)) || hasAssociation(name).size())
                    // we add executables to a separate list and place that list in front of the normal files
                    executable.emplace_front(qlname);
                else
                    l_completion.emplace_back(qlname);
            }
        }
        res=DOS_FindNext();
    }
    lfn_filefind_handle=fbak;
    /* Add executable list to front of completion list. */
    std::copy(executable.begin(),executable.end(),std::front_inserter(l_completion));
    dos.dta(save_dta);
    return true;
}

extern bool isKanji1(uint8_t chr), isKanji2(uint8_t chr);
extern bool CheckHat(uint8_t code);

static uint16_t GetWideCount(char *line, uint16_t str_index)
{
	bool kanji_flag = false;
	uint16_t count = 1;
	for(uint16_t pos = 0 ; pos < str_index ; pos++) {
		if(!kanji_flag) {
			if(isKanji1(line[pos])) {
				kanji_flag = true;
			}
			count = 1;
		} else {
			if(isKanji2(line[pos])) {
				count = 2;
			}
			kanji_flag = false;
		}
	}
	return count;
}

static uint16_t GetLastCount(char *line, uint16_t str_index)
{
	bool kanji_flag = false;
	uint16_t count = 1;
	for(uint16_t pos = 0 ; pos < str_index ; pos++) {
		if(!kanji_flag) {
			if(isKanji1(line[pos])) {
				kanji_flag = true;
			}
			count = 1;
		} else {
			if(isKanji2(line[pos])) {
				bool found=false;
				if (IS_PC98_ARCH && line[pos-1] == 0xFFFFFF86) {
                    for (auto it = pc98boxdrawmap.begin(); it != pc98boxdrawmap.end(); ++it)
                        if (it->second ==line[pos]) {
                            found=true;
                            break;
                        }
                }
				if (!found) count = 2;
			}
			kanji_flag = false;
		}
	}
	return count;
}

static uint16_t GetRemoveCount(char *line, uint16_t str_index)
{
	bool kanji_flag = false;
	uint16_t count = 1, total = 0;
	for(uint16_t pos = 0 ; pos < str_index ; pos++) {
		if(!kanji_flag) {
			if(isKanji1(line[pos])) {
				kanji_flag = true;
			}
			count = 1;
		} else {
			if(isKanji2(line[pos])) {
				bool found=false;
				if (IS_PC98_ARCH && line[pos-1] == 0xFFFFFF86) {
                    for (auto it = pc98boxdrawmap.begin(); it != pc98boxdrawmap.end(); ++it)
                        if (it->second ==line[pos]) {
                            found=true;
                            break;
                        }
                }
				count = found?0:1;
			}
			kanji_flag = false;
		}
		total += count;
	}
	return total;
}

static void RemoveAllChar(char *line, uint16_t str_index)
{
	for ( ; str_index > 0 ; str_index--) {
		// removes all characters
		if(CheckHat(line[str_index])) {
			backone(); backone(); outc(' '); outc(' '); backone(); backone();
		} else {
			backone(); outc(' '); backone();
		}
	}
	if(CheckHat(line[0])) {
		backone(); outc(' '); backone();
	}
}

static uint16_t DeleteBackspace(bool delete_flag, char *line, uint16_t &str_index, uint16_t &str_len)
{
	uint16_t count, pos, len;

	pos = str_index;
	if(delete_flag) {
		if(isKanji1(line[pos])&&line[pos+1]) {
			pos += 2;
		} else {
			pos += 1;
		}
	}
	count = GetWideCount(line, pos);

	pos = str_index;
	while(pos < str_len) {
		len = 1;
		DOS_WriteFile(STDOUT, (uint8_t *)&line[pos], &len);
		pos++;
	}
	if (delete_flag && str_index >= str_len)
		return 0;
	RemoveAllChar(line, GetRemoveCount(line, pos));
	pos = delete_flag ? str_index : str_index - count;
	while(pos < str_len - count) {
		line[pos] = line[pos + count];
		pos++;
	}
	line[pos] = 0;
	if(!delete_flag) {
		str_index -= count;
	}
	str_len -= count;
	len = str_len;
	DOS_WriteFile(STDOUT, (uint8_t *)line, &len);
	pos = GetRemoveCount(line, str_len);
	while(pos > str_index) {
		backone();
		if(CheckHat(line[pos - 1])) {
			backone();
		}
		pos--;
	}
	return count;
}

extern bool isDBCSCP();
void ReadCharAttr(uint16_t col, uint16_t row, uint8_t page, uint16_t* result);
bool read_lead_byte = false;
uint8_t temp_lead_byte;
/* NTS: buffer pointed to by "line" must be at least CMD_MAXLINE+1 large */
void DOS_Shell::InputCommand(char * line) {
	Bitu size=CMD_MAXLINE-2; //lastcharacter+0
	uint8_t c;uint16_t n=1;
	uint16_t str_len=0;uint16_t str_index=0;
	uint16_t len=0;
	bool current_hist=false; // current command stored in history?
    uint16_t cr;

#if defined(USE_TTF)
	if(IS_DOSV || IS_PC98_ARCH || ttf_dosv) {
#else
	if(IS_DOSV || IS_PC98_ARCH) {
#endif
		uint16_t int21_seg = mem_readw(0x0086);
		if(int21_seg != 0xf000) {
			if(real_readw(int21_seg - 1, 8) == 0x5a56) {
                // Vz editor resident
                real_writeb(cmd_line_seg, 0, 250);
				reg_dx = 0;
				reg_ah = 0x0a;
				SegSet16(ds, cmd_line_seg);
				CALLBACK_RunRealInt(0x21);
				str_len = real_readb(cmd_line_seg, 1);
				for(len = 0 ; len < str_len ; len++) {
					line[len] = real_readb(cmd_line_seg, 2 + len);
				}
				line[str_len] = '\0';
				return;
			}
		}
	}

    inshell = true;
    input_eof = false;
	line[0] = '\0';

	std::list<std::string>::iterator it_history = l_history.begin(), it_completion = l_completion.begin();

	while (size) {
		dos.echo=false;
		if (!DOS_ReadFile(input_handle,&c,&n)) {
            LOG(LOG_MISC,LOG_ERROR)("SHELL: Lost the input handle, dropping shell input loop");
            n = 0;
        }
		if (!n) {
            input_eof = true;
			size=0;			//Kill the while loop
			continue;
		}
        if (clearline) {
            clearline = false;
            *line=0;
            if (l_completion.size()) l_completion.clear(); //reset the completion list.
            str_index = 0;
            str_len = 0;
        }

        if (input_handle != STDIN) { /* FIXME: Need DOS_IsATTY() or somesuch */
            cr = (uint16_t)c; /* we're not reading from the console */
        }
        else if (IS_PC98_ARCH) {
            extern uint16_t last_int16_code;

            /* shift state is needed for some key combinations not directly supported by CON driver.
             * bit 4 = CTRL
             * bit 3 = GRPH/ALT
             * bit 2 = kana
             * bit 1 = caps
             * bit 0 = SHIFT */
            uint8_t shiftstate = mem_readb(0x52A + 0x0E);

            /* NTS: PC-98 keyboards lack the US layout HOME / END keys, therefore there is no mapping here */

            /* NTS: Since left arrow and backspace map to the same byte value, PC-98 treats it the same at the DOS prompt.
             *      However the PC-98 version of DOSKEY seems to be able to differentiate the two anyway and let the left
             *      arrow move the cursor back (perhaps it's calling INT 18h directly then?) */
                 if (c == 0x0B)
                cr = 0x4800;    /* IBM extended code up arrow */
            else if (c == 0x0A)
                cr = 0x5000;    /* IBM extended code down arrow */
                 else if (c == 0x0C) {
                     if (shiftstate & 0x10/*CTRL*/)
                         cr = 0x7400;    /* IBM extended code CTRL + right arrow */
                     else
                         cr = 0x4D00;    /* IBM extended code right arrow */
                 }
            else if (c == 0x08) {
                /* IBM extended code left arrow OR backspace. use last scancode to tell which as DOSKEY apparently can. */
                if (last_int16_code == 0x3B00) {
                    if (shiftstate & 0x10/*CTRL*/)
                        cr = 0x7300; /* CTRL + left arrow */
                    else
                        cr = 0x4B00; /* left arrow */
                }
                else {
                    cr = 0x08; /* backspace */
                }
            }
            else if (c == 0x1B) { /* escape */
                /* Either it really IS the ESC key, or an ANSI code */
                if (last_int16_code != 0x001B) {
                    DOS_ReadFile(input_handle,&c,&n);
                         if (c == 0x44)  // DEL
                        cr = 0x5300;
                    else if (c == 0x50)  // INS
                        cr = 0x5200;
                    else if (c == 0x53)  // F1
                        cr = 0x3B00;
                    else if (c == 0x54)  // F2
                        cr = 0x3C00;
                    else if (c == 0x55)  // F3
                        cr = 0x3D00;
                    else if (c == 0x56)  // F4
                        cr = 0x3E00;
                    else if (c == 0x57)  // F5
                        cr = 0x3F00;
                    else if (c == 0x45)  // F6
                        cr = 0x4000;
                    else if (c == 0x4A)  // F7
                        cr = 0x4100;
                    else if (c == 0x51)  // F9
                        cr = 0x4300;
                    else if (c == 0x5A)  // F10
                        cr = 0x4400;
                    else
                        cr = 0;
                }
                else {
                    cr = (uint16_t)c;
                }
            }
            else {
                cr = (uint16_t)c;
            }
        }
        else {
            if (c == 0) {
				DOS_ReadFile(input_handle,&c,&n);
                cr = (uint16_t)c << (uint16_t)8;
            }
            else {
                cr = (uint16_t)c;
            }
        }

#if defined(USE_TTF)
        if (ttf.inUse && rtl) {
            if (cr == 0x4B00) cr = 0x4D00;
            else if (cr == 0x4D00) cr = 0x4B00;
            else if (cr == 0x7300) cr = 0x7400;
            else if (cr == 0x7400) cr = 0x7300;
        }
#endif
        uint8_t page, col, row;
        switch (cr) {
            case 0x3d00:	/* F3 */
                if (!l_history.size()) break;
                it_history = l_history.begin();
                if (it_history != l_history.end() && it_history->length() > str_len) {
                    const char *reader = &(it_history->c_str())[str_len];
                    while((c = (uint8_t)(*reader++))) {
                        line[str_index++] = (char)c;
                    }
                    str_len = str_index = (uint16_t)it_history->length();
                    size = (unsigned int)CMD_MAXLINE - str_index - 2u;
                    line[str_len] = 0;
                    DOS_WriteFile(STDOUT, (const uint8_t *)it_history->c_str(), &str_len);
                }
                break;

            case 0x4B00:	/* LEFT */
                if(IS_PC98_ARCH
#if defined(USE_TTF)
                    && dbcs_sbcs
#endif
                    ) {
                    if (str_index) {
                        uint16_t count = GetLastCount(line, str_index);
                        uint8_t ch = line[str_index - 1];
                        while(count > 0) {
                            uint16_t wide = GetWideCount(line, str_index);
                            backone();
                            str_index --;
                            if (wide > count) str_index --;
                            count--;
                        }
                        if(CheckHat(ch)) {
                            backone();
                        }
                    }
                    page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
                    col = CURSOR_POS_COL(page);
                    row = CURSOR_POS_ROW(page);
                    uint16_t get_char, get_char2;
                    ReadCharAttr(col, row, page, &get_char);
                    if((line[str_index] & 0xFF) != (uint8_t)(get_char & 0xFF)) {
                        ReadCharAttr(col-1, row, page, &get_char2);
                        if((uint8_t)(get_char2 & 0xFF) == (line[str_index] & 0xFF)) INT10_SetCursorPos(row, col - 1, page);
                    }
                } else {
                    if (isDBCSCP()
#if defined(USE_TTF)
                        &&dbcs_sbcs
#endif
                        &&str_index>1&&(line[str_index-1]<0||((dos.loaded_codepage==932||(dos.loaded_codepage==936&&gbk)||dos.loaded_codepage==950||dos.loaded_codepage==951)&&line[str_index-1]>=0x40))&&line[str_index-2]<0) {
                        backone();
                        str_index --;
                        MoveCaretBackwards();
                        page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
                        col = CURSOR_POS_COL(page);
                        row = CURSOR_POS_ROW(page);
                        uint16_t get_char, get_char2;
                        ReadCharAttr(col, row, page, &get_char);
                        if((line[str_index] & 0xFF) != (uint8_t)(get_char & 0xFF)) {
                            ReadCharAttr(col - 1, row, page, &get_char2);
                            if((uint8_t)(get_char2 & 0xFF) == (line[str_index] & 0xFF)) INT10_SetCursorPos(row, col - 1, page);
                        }
                        uint8_t iskanji = 0;
                        col = CURSOR_POS_COL(page);
                        if(col > 0 && str_index > 0) {
                            for(uint16_t i = 0; i < str_index; i++) {
                                if(isKanji1(line[i]) && isKanji2(line[i + 1])) {
                                    if(i + 1 == str_index) iskanji = 2;
                                    else i++; // skip next character
                                }
                            }

                            if(iskanji == 2) {
                                backone();
                                str_index--;
                            }
                        }
                    }
                    else if (str_index) {
                        backone();
                        str_index --;
                        MoveCaretBackwards();
                    }
                }
                break;

			case 0x7400: /*CTRL + RIGHT : cmd.exe-like next word*/
				{
					auto pos = line + str_index;
					auto spc = *pos == ' ';
					const auto end = line + str_len;

					while (pos < end) {
						if (spc && *pos != ' ')
							break;
						if (*pos == ' ')
							spc = true;
						pos++;
					}
					
					const auto lgt = MIN(pos, end) - (line + str_index);
					
					for (auto i = 0; i < lgt; i++)
						outc(static_cast<uint8_t>(line[str_index++]));
				}	
        		break;
			case 0x7300: /*CTRL + LEFT : cmd.exe-like previous word*/
				{
					auto pos = line + str_index - 1;
					const auto beg = line;
					const auto spc = *pos == ' ';

					if (spc) {
						while(*pos == ' ') pos--;
						while(*pos != ' ') pos--;
						pos++;
					}
					else {
						while(*pos != ' ') pos--;
						pos++;
					}
					
					const auto lgt = std::abs(MAX(pos, beg) - (line + str_index));
					
					for (auto i = 0; i < lgt; i++) {
						backone();
						str_index--;
						MoveCaretBackwards();
					}
				}	
        		break;
            case 0x4D00:	/* RIGHT */
                if(IS_PC98_ARCH || (isDBCSCP()
#if defined(USE_TTF)
                    && dbcs_sbcs
#endif
                    && IS_DOS_JAPANESE)) {
                    if (str_index < str_len) {
                        uint16_t count = 1;
                        if(str_index < str_len - 1) {
                            count = GetLastCount(line, str_index + 2);
                        }
                        while(count > 0) {
                            outc(line[str_index++]);
                            count--;
                        }
                    }
                    page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
                    col = CURSOR_POS_COL(page);
                    BIOS_NCOLS;
                    if(col == ncols - 1 && str_index < str_len && isKanji1(line[str_index]) && isKanji2(line[str_index+1])) {
                        row = CURSOR_POS_ROW(page);
                        INT10_SetCursorPos(row+1, 0, page);
                    }
                } else {
                    if (isDBCSCP()
#if defined(USE_TTF)
                        &&dbcs_sbcs
#endif
                        &&str_index<str_len-1&&line[str_index]<0&&(line[str_index+1]<0||((dos.loaded_codepage==932||(dos.loaded_codepage==936&&gbk)||dos.loaded_codepage==950||dos.loaded_codepage==951)&&line[str_index+1]>=0x40))) {
                        if(isKanji1((uint8_t)line[str_index]) && isKanji2((uint8_t)line[str_index + 1])) {
                            const uint8_t buf[2] = {(uint8_t)line[str_index],(uint8_t)line[str_index + 1]};
                            uint16_t num = 2;
                            DOS_WriteFile(STDOUT, buf, &num);
                            str_index += 2;
                        }
                        else {
                            outc((uint8_t)line[str_index]);
                            str_index++;
                        }
                    }
                    else if (str_index < str_len) {
                        outc((uint8_t)line[str_index++]);
                    }
                }
                break;

            case 0x4700:	/* HOME */
                while (str_index) {
                    backone();
                    str_index--;
                }
                page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
                col = CURSOR_POS_COL(page);
                while(col > prompt_col) {
                    backone();
                    col = CURSOR_POS_COL(page);
                }
                break;

            case 0x5200:    /* INS */
                if (IS_PC98_ARCH) { // INS state handled by IBM PC/AT BIOS, faked for PC-98 mode
                    extern bool pc98_doskey_insertmode;

                    // NTS: No visible change to the cursor, just like DOSKEY on PC-98 MS-DOS
                    pc98_doskey_insertmode = !pc98_doskey_insertmode;
                }
                break;

            case 0x4F00:	/* END */
                {
                    uint16_t a = str_len - str_index;
                    uint8_t* text = reinterpret_cast<uint8_t*>(&line[str_index]);
                    DOS_WriteFile(STDOUT, text, &a);//write buffer to screen
                    str_index = str_len;
                }
                break;

            case 0x4800:	/* UP */
                if (l_history.empty() || it_history == l_history.end()) break;

                // store current command in history if we are at beginning
                if (it_history == l_history.begin() && !current_hist) {
                    current_hist=true;
                    l_history.emplace_front(line);
                }

                // ensure we're at end to handle all cases
                while (str_index < str_len) {
                    outc((uint8_t)line[str_index++]);
                }

                for (;str_index>0; str_index--) {
                    // removes all characters
                    backone(); outc(' '); backone();
                }
                page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
                col = CURSOR_POS_COL(page);
                while(col > prompt_col){
                    backone(); outc(' '); backone();
                    col = CURSOR_POS_COL(page);
                }

                strcpy(line, it_history->c_str());
                len = (uint16_t)it_history->length();
                str_len = str_index = len;
                size = (unsigned int)CMD_MAXLINE - str_index - 2u;
                DOS_WriteFile(STDOUT, (uint8_t *)line, &len);
                ++it_history;
                break;

            case 0x5000:	/* DOWN */
                if (l_history.empty() || it_history == l_history.begin()) break;

                // not very nice but works ..
                --it_history;
                if (it_history == l_history.begin()) {
                    // no previous commands in history
                    ++it_history;

                    // remove current command from history
                    if (current_hist) {
                        current_hist=false;
                        l_history.pop_front();
                    }
                    break;
                } else --it_history;

                // ensure we're at end to handle all cases
                while (str_index < str_len) {
                    outc((uint8_t)line[str_index++]);
                }

                for (;str_index>0; str_index--) {
                    // removes all characters
                    backone(); outc(' '); backone();
                }
                page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
                col = CURSOR_POS_COL(page);
                while(col > prompt_col) {
                    backone(); outc(' '); backone();
                    col = CURSOR_POS_COL(page);
                }

                strcpy(line, it_history->c_str());
                len = (uint16_t)it_history->length();
                str_len = str_index = len;
                size = (unsigned int)CMD_MAXLINE - str_index - 2u;
                DOS_WriteFile(STDOUT, (uint8_t *)line, &len);
                ++it_history;

                break;
            case 0x5300:/* DELETE */
                if(IS_PC98_ARCH
#if defined(USE_TTF)
                    && dbcs_sbcs
#endif
                    ) {
                    if(str_len) {
                        size += DeleteBackspace(true, line, str_index, str_len);
                    }
                } else {
                    if(str_index>=str_len) break;
                    uint8_t k=1;
                    if (isDBCSCP()
#if defined(USE_TTF)
                        &&dbcs_sbcs
#endif
                        &&str_index<str_len-1&&line[str_index]<0&&(line[str_index+1]<0||((dos.loaded_codepage==932||(dos.loaded_codepage==936&&gbk)||dos.loaded_codepage==950||dos.loaded_codepage==951)&&line[str_index+1]>=0x40)))
                        k=2;
                    for(uint16_t i = str_index; i < (str_len - k); i++) {
                        line[i] = line[i + k];
                    }
                    line[str_len - k] = 0;
                    line[str_len - k + 1] = 0;
                    line[str_len] = 0;
                    str_len -= k;
                    uint16_t a=str_len-str_index;
                    uint8_t* text=reinterpret_cast<uint8_t*>(&line[str_index]);
                    page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
                    BIOS_NCOLS; BIOS_NROWS;
                    col = CURSOR_POS_COL(page);
                    DOS_WriteFile(STDOUT,text,&a);//write buffer to screen
                    uint16_t col2 = CURSOR_POS_COL(page);
                    row = CURSOR_POS_ROW(page);
                    if(k == 2){
                        outc(' '); outc(' '); backone(); backone();
                    }
                    else {
                        outc(' '); backone();
                    }
                    if(col2 >= ncols - (k==2?3:1) && row < nrows) {
                        for(int i = 0; i <= k; i++) WriteChar(i, row + 1, page, ' ', 0, false);
                    }
                    col2 = CURSOR_POS_COL(page); //LOG_MSG("col=%d, a=%d", col,a);
                    if(col2 >= a) INT10_SetCursorPos(row, col2 - a, page);
                    else {
                        uint16_t lines_up = (a - col2 - 1) / ncols + 1;
                        if(col2 < ncols) for(int i = col2; i < ncols; i++) WriteChar(i, row, page, ' ', 0, false);
                        if(row >= lines_up) INT10_SetCursorPos(row - lines_up, col, page);
                        else INT10_SetCursorPos(0, 0, page);
                    }
                    size += k;
                }
                break;
            case 0x0F00:	/* Shift-Tab */
                if (!l_completion.size()) {
                    if (BuildCompletions(line, str_len))
                        it_completion = l_completion.end();
                    else
                        break;
                }
                if (l_completion.size()) {
                    if (it_completion == l_completion.begin()) it_completion = l_completion.end (); 
                    --it_completion;

                    if (it_completion->length()) {
                        for (;str_index > completion_index; str_index--) {
                            // removes all characters
                            backone(); outc(' '); backone();
                        }

                        strcpy(&line[completion_index], it_completion->c_str());
                        len = (uint16_t)it_completion->length();
                        str_len = str_index = (Bitu)(completion_index + len);
                        size = (unsigned int)CMD_MAXLINE - str_index - 2u;
                        DOS_WriteFile(STDOUT, (uint8_t *)it_completion->c_str(), &len);
                    }
                }
                break;
            case 0x08:				/* BackSpace */
                if(IS_PC98_ARCH
#if defined(USE_TTF)
                    && dbcs_sbcs
#endif
                    ) {
                    if(str_index) {
                        size += DeleteBackspace(false, line, str_index, str_len);
                    }
                }
                else {
                    if(str_index == 0)break;
                    page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
                    uint8_t bytes = 1; // Previous character is DBCS or SBCS
                    if(isDBCSCP()
#if defined(USE_TTF)
                        && dbcs_sbcs
#endif
                        ) bytes = GetLastCount(line, str_index);
                    
                    if(str_index < str_len) {
                        uint16_t a = str_len - str_index;
                        uint8_t* text = reinterpret_cast<uint8_t*>(&line[str_index]);
                        DOS_WriteFile(STDOUT, text, &a);
                    }
                    backone();
                    if(bytes == 2 && str_len >= 2) backone();
                    outc(' ');
                    if(bytes == 2 && str_len >= 2) outc(' ');
                    backone();
                    if(bytes == 2 && str_len >= 2) backone();

                    if(str_index < str_len)
                        memmove(&line[str_index - bytes], &line[str_index], str_len - str_index);  // Shift buffer to left

                    line[str_len - 1] = '\0';
                    if(bytes == 2)line[str_len - 2] = '\0';
                    str_len -= bytes;
                    str_index -= bytes;
                    size -= bytes;

                    for(int i = str_len; i > 0; i--) backone();
                    col = CURSOR_POS_COL(page);
                    while(col > prompt_col) {
                        backone();
                        col = CURSOR_POS_COL(page);
                    }

                    uint16_t a = str_len;
                    uint8_t* text = reinterpret_cast<uint8_t*>(&line[0]);
                    DOS_WriteFile(STDOUT, text, &a); // Rewrite the command history
                    outc(' '); outc(' ');  backone(); backone();

                    if(str_index < str_len) {
                        for(int i = str_len; i > 0; i--) backone();
                        col = CURSOR_POS_COL(page);
                        while(col > prompt_col) {
                            backone();
                            col = CURSOR_POS_COL(page);
                        }

                        a = str_index;
                        text = reinterpret_cast<uint8_t*>(&line[0]);
                        DOS_WriteFile(STDOUT, text, &a); // Move to the cursor position
                    }
                }
                if (l_completion.size()) l_completion.clear();
                break;
            case 0x0a:				/* Give a new Line */
                outc('\n');
                break;
            case '': // FAKE CTRL-C
                *line = 0;      // reset the line.
                if (l_completion.size()) l_completion.clear(); //reset the completion list.
                size = 0;       // stop the next loop
                str_len = 0;    // prevent multiple adds of the same line
                DOS_BreakFlag = false; // clear break flag so the next program doesn't get hit with it
                DOS_BreakConioFlag = false;
                break;
            case 0x0d:				/* Don't care, and return */
            {
                uint16_t a = str_len - str_index;
                uint8_t* text = reinterpret_cast<uint8_t*>(&line[str_index]);
                DOS_WriteFile(STDOUT, text, &a);//goto end of command line
                str_index = str_len;
            }
                if(!echo) { outc('\r'); outc('\n'); }
                size=0;			//Kill the while loop
                break;
            case 0x9400:	/* Ctrl-Tab */
            {
                if (!l_completion.size()) {
                    if (BuildCompletions(line, str_len))
                        it_completion = l_completion.begin();
                    else
                        break;
                }
                size_t w_count, p_count, col;
                unsigned int max[15], total, tcols=IS_PC98_ARCH?80:real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
                if (!tcols) tcols=80;
                int mrow=tcols>80?15:10;
                for (col=mrow; col>0; col--) {
                    for (int i=0; i<mrow; i++) max[i]=2;
                    if (col==1) break;
                    w_count=0;
                    for (std::list<std::string>::iterator source = l_completion.begin(); source != l_completion.end(); ++source) {
                        std::string name = source->c_str();
                        if (name.size()+2>max[w_count%col]) max[w_count%col]=(unsigned int)(name.size()+2);
                        ++w_count;
                    }
                    total=0;
                    for (size_t i=0; i<col; i++) total+=max[i];
                    if (total<tcols) break;
                }
                w_count = p_count = 0;
                bool lastcr=false;
                if (l_completion.size()) {WriteOut_NoParsing("\n");lastcr=true;}
                for (std::list<std::string>::iterator source = l_completion.begin(); source != l_completion.end(); ++source) {
                    std::string name = source->c_str();
                    if (col==1) {
                        WriteOut_NoParsing(name.c_str(), true);
                        WriteOut("\n");
                        lastcr=true;
                        p_count++;
                    } else {
                        WriteOut("%s%-*s", name.c_str(), max[w_count % col]-name.size(), "");
                        lastcr=false;
                    }
                    if (col>1) {
                        ++w_count;
                        if (w_count % col == 0) {p_count++;WriteOut_NoParsing("\n");lastcr=true;}
                    }
                    if (p_count>GetPauseCount()) {
                        WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
                        lastcr=false;
                        w_count = p_count = 0;
                        uint8_t c;uint16_t n=1;
                        DOS_ReadFile(STDIN,&c,&n);
                        if (c==3) {ctrlbrk=false;WriteOut("^C\r\n");break;}
                        if (c==0) DOS_ReadFile(STDIN,&c,&n);
                    }
                }
                if (l_completion.size()) {
                    if (!lastcr) WriteOut_NoParsing("\n");
                    ShowPrompt();
                    WriteOut("%s", line);
                }
                break;
            }
            case'\t':
                if (l_completion.size()) {
                    ++it_completion;
                    if (it_completion == l_completion.end()) it_completion = l_completion.begin();
                } else if (BuildCompletions(line, str_len))
                    it_completion = l_completion.begin();
                else
                    break;

                if (l_completion.size() && it_completion->length()) {
                    for (;str_index > completion_index; str_index--) {
                        // removes all characters
                        backone(); outc(' '); backone();
                    }

                    strcpy(&line[completion_index], it_completion->c_str());
                    len = (uint16_t)it_completion->length();
                    str_len = str_index = (Bitu)(completion_index + len);
                    size = (unsigned int)CMD_MAXLINE - str_index - 2u;
                    DOS_WriteFile(STDOUT, (uint8_t *)it_completion->c_str(), &len);
                }
                break;
            case 0x1b:   /* ESC */
                // NTS: According to real PC-98 DOS:
                //      If DOSKEY is loaded, ESC clears the prompt
                //      If DOSKEY is NOT loaded, ESC does nothing. In fact, after ESC,
                //      the next character input is thrown away before resuming normal keyboard input.
                //
                //      DOSBox / DOSBox-X have always acted as if DOSKEY is loaded in a fashion, so
                //      we'll emulate the PC-98 DOSKEY behavior here.
                //
                //      DOSKEY on PC-98 is able to clear the whole prompt and even bring the cursor
                //      back up to the first line if the input crosses multiple lines.

                // NTS: According to real IBM/Microsoft PC/AT DOS:
                //      If DOSKEY is loaded, ESC clears the prompt
                //      If DOSKEY is NOT loaded, ESC prints a backslash and goes to the next line.
                //      The Windows 95 version of DOSKEY puts the cursor at a horizontal position
                //      that matches the DOS prompt (not emulated here).
                //
                //      DOSBox / DOSBox-X have always acted as if DOSKEY is loaded in a fashion, so
                //      we'll emulate DOSKEY behavior here.
                while (str_index < str_len) {
                    uint16_t count = 1, wide = 1;
                    if(IS_PC98_ARCH || (isDBCSCP()
#if defined(USE_TTF)
                        && dbcs_sbcs
#endif
                        && IS_DOS_JAPANESE)) {
                            count = GetLastCount(line, str_index+1);
                            wide = GetWideCount(line, str_index+1);
                    }
                    outc(' ');
                    str_index++;
                    if (wide > count) str_index ++;
                }
                while (str_index > 0) {
                    uint16_t count = 1, wide = 1;
                if(IS_PC98_ARCH || (isDBCSCP()
#if defined(USE_TTF)
                    && dbcs_sbcs
#endif
                    && IS_DOS_JAPANESE)) {
                        count = GetLastCount(line, str_index);
                        wide = GetWideCount(line, str_index);
                    }
                    backone();
                    outc(' ');
                    backone();
                    MoveCaretBackwards();
                    str_index--;
                    if (wide > count) str_index --;
                }

                *line = 0;      // reset the line.
                if (l_completion.size()) l_completion.clear(); //reset the completion list.
                str_index = 0;
                str_len = 0;
                break;
            default:
                if(IS_PC98_ARCH  
#if defined(USE_TTF)
                    && dbcs_sbcs
#endif
                    ) {
                    if(dos.loaded_codepage == 932 && isKanji1(cr >> 8) && (cr & 0xFF) == 0) break;
                    bool kanji_flag = false;
                    uint16_t pos = str_index;
                    while(1) {
                        if (l_completion.size()) l_completion.clear();
                        if(str_index < str_len && true) {
                            for(Bitu i=str_len;i>str_index;i--) {
                                line[i]=line[i-1];
                            }
                            line[++str_len]=0;
                            size--;
                        }
                        line[str_index]=c;
                        str_index ++;
                        if (str_index > str_len) {
                            line[str_index] = '\0';
                            str_len++;
                            size--;
                        }
                        if(!isKanji1(c) || kanji_flag) {
                            break;
                        }
                        DOS_ReadFile(input_handle,&c,&n);
                        kanji_flag = true;
                    }
                    while(pos < str_len) {
                        outc(line[pos]);
                        pos++;
                    }
                    pos = GetRemoveCount(line, str_len);
                    while(pos > str_index) {
                        backone();
                        pos--;
                        if (CheckHat(line[pos])) {
                            backone();
                        }
                    }
                } else {
                    if(cr >= 0x100) break;
                    if(l_completion.size()) l_completion.clear();

                    page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
                    if(str_index < str_len) {  // Insert Mode Handling
                        col = CURSOR_POS_COL(page);
                        bool delete_lead_byte = false;
                        if(INT10_GetInsertState()) {
                            for(uint16_t i = str_len; i > str_index; i--) {
                                line[i] = line[i - 1];  // Shift buffer to the right
                            }
                            line[++str_len] = '\0';  // New buffer end
                            size--;
                        }
                        else {
                            if(isDBCSCP()) {
                                if(read_lead_byte) {
                                    if(!isKanji1(line[str_index])) {
                                        for(uint16_t i = str_len; i > str_index; i--) {
                                            line[i] = line[i - 1];  // Shift buffer to the right
                                        }
                                        line[++str_len] = '\0';  // New buffer end
                                        size--;
                                    }
                                    line[str_index] = temp_lead_byte;
                                    line[str_index + 1] = (char)(cr & 0xFF);
                                }
                                else if(!isKanji1(cr & 0xFF) && isKanji1(line[str_index])) {
                                    for(uint16_t i = str_index + 1; i < str_len; i++) {
                                        line[i] = line[i + 1];  // Delete trailing byte of DBCS since leading byte is overwritten
                                    }
                                    read_lead_byte = false;
                                    delete_lead_byte = true;
                                    line[str_len] = ' ';
                                    size++;
                                }
                                else if(isKanji1(cr & 0xFF)) {
                                    temp_lead_byte = (char)(cr & 0xFF);
                                    read_lead_byte = true;
                                    continue;
                                }
                            }
                            else read_lead_byte = false;
                        }
                        if(!read_lead_byte)line[str_index] = (char)(cr & 0xFF);
                        uint16_t a = str_len - str_index;
                        uint8_t* text = reinterpret_cast<uint8_t*>(&line[str_index]);
                        DOS_WriteFile(STDOUT, text, &a);  // Write remaining buffer to screen
                        str_index++;
                        if(read_lead_byte) {
                            str_index++;
                            col++;
                            read_lead_byte = false;
                        }
                        if(delete_lead_byte) {
                            line[str_len] = '\0';  // Ensure null-terminated
                            str_len--;
                            outc(' '); outc(' '); backone(); backone();
                        }

                        for(uint16_t i = str_len; i > str_index; i--) {
                            backone();
                        }
                        uint8_t col2 = CURSOR_POS_COL(page);
                        while(col2 > col+1) {
                            backone();
                            col2 = CURSOR_POS_COL(page);
                        }
                        break;
                    }

                    // Insert the new character
                    line[str_index] = (char)(cr & 0xFF);
                    str_index++;
                    if(str_index > str_len) {
                        line[str_index] = '\0';  // Null-terminate if new end
                        str_len++;
                        size--;
                    }
                    else if(!INT10_GetInsertState()) {
                        // Overwrite mode: just move to the next position
                        if(str_index == str_len) {
                            line[str_index] = '\0';  // Ensure null-terminated
                            str_len++;
                            size--;
                        }
                    }

                    // Output the inserted character
                    outc((uint8_t)(cr & 0xFF));
                }
                break;
        }
    }

    inshell = false;
	if (!str_len) return;
	str_len++;

	// remove current command from history if it's there
	if (current_hist) {
		// current_hist=false;
		l_history.pop_front();
	}

	// add command line to history. Win95 behavior with DOSKey suggests
	// that the original string is preserved, not the expanded string.
	l_history.emplace_front(line); it_history = l_history.begin();
	if (l_completion.size()) l_completion.clear();

	/* DOS %variable% substitution */
	ProcessCmdLineEnvVarStitution(line);
}

void XMS_DOS_LocalA20DisableIfNotEnabled(void);

/* Note: Buffer pointed to by "line" must be at least CMD_MAXLINE+1 bytes long! */
void DOS_Shell::ProcessCmdLineEnvVarStitution(char *line) {
    // Wengier: Rewrote the code using regular expressions (a lot shorter)
    // Tested by both myself and kcgen with various boundary cases

    /* DOS7/Win95 testing:
     *
     * "%" = "%"
     * "%%" = "%"
     * "% %" = ""
     * "%  %" = ""
     * "% % %" = " %"
     *
     * ^ WTF?
     *
     * So the below code has funny conditions to match Win95's weird rules on what
     * constitutes valid or invalid %variable% names. */

    /* continue scanning for the ending '%'. variable names are apparently meant to be
     * alphanumeric, start with a letter, without spaces (if Windows 95 COMMAND.COM is
     * any good example). If it doesn't end in '%' or is broken by space or starts with
     * a number, substitution is not carried out. In the middle of the variable name
     * it seems to be acceptable to use hyphens.
     *
     * since spaces break up a variable name to prevent substitution, these commands
     * act differently from one another:
     *
     * C:\>echo %PATH%
     * C:\DOS;C:\WINDOWS
     *
     * C:\>echo %PATH %
     * %PATH % */

	/* initial scan: is there anything to substitute? */
	/* if not, then just return without modifying "line" */
	/* not really needed but keep this code as is for now */
	char *r=line;
	while (*r != 0 && *r != '%') r++;
	if (*r != '%') return;

	/* if the incoming string is already too long, then that's a problem too! */
	if (((size_t)(r+1-line)) >= CMD_MAXLINE) {
		LOG_MSG("DOS_Shell::ProcessCmdLineEnvVarStitution WARNING incoming string to substitute is already too long!\n");
		*line = 0; /* clear string (C-string chop with NUL) */
		WriteOut("Command input error: string expansion overflow\n");
		return;
	}

    // Begin the process of shell variable substitutions using regular expressions
    // Variable names must start with non-digits. Space and special symbols like _ and ~ are apparently valid too (MS-DOS 7/Windows 9x).
	constexpr char surrogate_percent = 8;
	const static std::regex re("\\%([^%0-9][^%]*)?%");
	bool isfor = !strncasecmp(ltrim(line),"FOR ",4);
	char *p = isfor?strchr(line, '%'):NULL;
	std::string text = p?p+1:line;
	std::smatch match;
	/* Iterate over potential %var1%, %var2%, etc matches found in the text string */
	while (std::regex_search(text, match, re)) {
		// Get the first matching %'s position and length
		const auto percent_pos = static_cast<size_t>(match.position(0));
		const auto percent_len = static_cast<size_t>(match.length(0));
		std::string variable_name = match[1].str();
		if (variable_name.empty()) {
			/* Replace %% with the character "surrogate_percent", then (eventually) % */
			text.replace(percent_pos, percent_len, std::string(1, surrogate_percent));
			continue;
		}
		/* Trim preceding spaces from the variable name */
		variable_name.erase(0, variable_name.find_first_not_of(' '));
		std::string variable_value;
		if (variable_name.size() && GetEnvStr(variable_name.c_str(), variable_value)) {
			const size_t equal_pos = variable_value.find_first_of('=');
			/* Replace the original %var% with its corresponding value from the environment */
			const std::string replacement = equal_pos != std::string::npos ? variable_value.substr(equal_pos + 1) : "";
			text.replace(percent_pos, percent_len, replacement);
		} else
			text.replace(percent_pos, percent_len, "");
	}
	std::replace(text.begin(), text.end(), surrogate_percent, '%');
	if (p) {
		*(p+1) = 0;
		text = std::string(line) + text;
	}
	assert(text.size() <= CMD_MAXLINE);
	strcpy(line, text.c_str());
}

int infix=-1;
std::string full_arguments = "";
bool dos_a20_disable_on_exec=false;
extern bool packerr, mountwarning, nowarn;
bool DOS_Shell::Execute(char* name, const char* args) {
/* return true  => don't check for hardware changes in do_command 
 * return false =>       check for hardware changes in do_command */
	char fullname[DOS_PATHLENGTH+4]; //stores results from Which
	const char* p_fullname;
	char line[CMD_MAXLINE];
	if(strlen(args)!= 0){
		if(*args != ' '){ //put a space in front
			line[0]=' ';line[1]=0;
			strncat(line,args,CMD_MAXLINE-2);
			line[CMD_MAXLINE-1]=0;
		}
		else
		{
			safe_strncpy(line,args,CMD_MAXLINE);
		}
	}else{
		line[0]=0;
	}

	const Section_prop* sec = static_cast<Section_prop*>(control->GetSection("dos"));
	/* check for a drive change */
	if (((strcmp(name + 1, ":") == 0) || (strcmp(name + 1, ":\\") == 0)) && isalpha(*name))
	{
#ifdef WIN32
		uint8_t c;uint16_t n;
#endif
		if (strrchr_dbcs(name,'\\')) { WriteOut(MSG_Get("SHELL_EXECUTE_ILLEGAL_COMMAND"),name); return true; }
		if (!DOS_SetDrive(toupper(name[0])-'A')) {
#ifdef WIN32
			if(!sec->Get_bool("automount")) { WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),toupper(name[0])); return true; }
			// automount: attempt direct letter to drive map.
			int type=GetDriveType(name);
			if(!mountwarning && type!=DRIVE_NO_ROOT_DIR) goto continue_1;
			if(type==DRIVE_FIXED && (strcasecmp(name,"C:")==0)) WriteOut(MSG_Get("PROGRAM_MOUNT_WARNING_WIN"));
first_1:
			if(type==DRIVE_CDROM) WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_ACCESS_CDROM"),toupper(name[0]));
			else if(type==DRIVE_REMOVABLE && (strcasecmp(name,"A:")==0||strcasecmp(name,"B:")==0)) WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_ACCESS_FLOPPY"),toupper(name[0]));
			else if(type==DRIVE_REMOVABLE) WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_ACCESS_REMOVABLE"),toupper(name[0]));
			else if(type==DRIVE_REMOTE) WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_ACCESS_NETWORK"),toupper(name[0]));
			else if(type==DRIVE_FIXED||type==DRIVE_RAMDISK||type==DRIVE_UNKNOWN) WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_ACCESS_FIXED"),toupper(name[0]));
			else { WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),toupper(name[0])); return true; }

first_2:
		n=1;
		DOS_ReadFile (STDIN,&c,&n);
		do switch (c) {
			case 'n':			case 'N':
			{
				DOS_WriteFile (STDOUT,&c, &n);
				DOS_ReadFile (STDIN,&c,&n);
				do switch (c) {
					case 0xD: WriteOut("\n\n"); WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),toupper(name[0])); return true;
					case 0x3: return true;
					case 0x8: WriteOut("\b \b"); goto first_2;
				} while (DOS_ReadFile (STDIN,&c,&n));
			}
			case 'y':			case 'Y':
			{
				DOS_WriteFile (STDOUT,&c, &n);
				DOS_ReadFile (STDIN,&c,&n);
				do switch (c) {
					case 0xD: WriteOut("\n"); goto continue_1;
					case 0x3: return true;
					case 0x8: WriteOut("\b \b"); goto first_2;
				} while (DOS_ReadFile (STDIN,&c,&n));
			}
			case 0x3: return true;
			case 0xD: WriteOut("\n"); goto first_1;
			case '\t': case 0x08: goto first_2;
			default:
			{
				DOS_WriteFile (STDOUT,&c, &n);
				DOS_ReadFile (STDIN,&c,&n);
				do switch (c) {
					case 0xD: WriteOut("\n");goto first_1;
					case 0x3: return true;
					case 0x8: WriteOut("\b \b"); goto first_2;
				} while (DOS_ReadFile (STDIN,&c,&n));
				goto first_2;
			}
		} while (DOS_ReadFile (STDIN,&c,&n));

continue_1:

			char mountstring[DOS_PATHLENGTH+CROSS_LEN+20];
			sprintf(mountstring,"MOUNT %s ",name);

			if(type==DRIVE_CDROM) strcat(mountstring,"-t cdrom ");
			else if(type==DRIVE_REMOVABLE && (strcasecmp(name,"A:")==0||strcasecmp(name,"B:")==0)) strcat(mountstring,"-t floppy ");
			strcat(mountstring,name);
			strcat(mountstring,"\\");
//			if(GetDriveType(name)==5) strcat(mountstring," -ioctl");
			nowarn=true;
			this->ParseLine(mountstring);
            nowarn=false;
//failed:
			if (!DOS_SetDrive(toupper(name[0])-'A'))
#endif
			WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),toupper(name[0]));
		}
		return true;
	}
	/* Check for a full name */
	p_fullname = Which(name);
	if (!p_fullname) return false;
	strcpy(fullname,p_fullname);
    std::string assoc = hasAssociation(fullname);
    if (assoc.size()) {
        noassoc=true;
        DoCommand((char *)(assoc+" "+fullname).c_str());
        noassoc=false;
        return true;
    }
	const char* extension = strrchr(fullname,'.');

	/*always disallow files without extension from being executed. */
	/*only internal commands can be run this way and they never get in this handler */
	if (!extension) {
		//Check if the result will fit in the parameters. Else abort
		if(strlen(fullname) >( DOS_PATHLENGTH - 1) ) return false;
        char temp_name[DOS_PATHLENGTH + 4];
        const char* temp_fullname;
		//try to add .com, .exe and .bat extensions to filename
		
		strcpy(temp_name,fullname);
		strcat(temp_name,".COM");
		temp_fullname=Which(temp_name);
		if (temp_fullname) { extension=".com";strcpy(fullname,temp_fullname); }

		else 
		{
			strcpy(temp_name,fullname);
			strcat(temp_name,".EXE");
			temp_fullname=Which(temp_name);
		 	if (temp_fullname) { extension=".exe";strcpy(fullname,temp_fullname);}

			else 
			{
				strcpy(temp_name,fullname);
				strcat(temp_name,".BAT");
				temp_fullname=Which(temp_name);
		 		if (temp_fullname) { extension=".bat";strcpy(fullname,temp_fullname);}

				else  
				{
		 			return false;
				}
			
			}	
		}
	}
	
	if (strcasecmp(extension, ".bat") == 0) 
	{	/* Run the .bat file */
		/* delete old batch file if call is not active*/
		bool temp_echo=echo; /*keep the current echostate (as delete bf might change it )*/
		if(bf && !call) delete bf;
		bf=new BatchFile(this,fullname,name,line);
		echo=temp_echo; //restore it.
	} 
	else 
	{	/* only .bat .exe .com extensions maybe be executed by the shell */
		if(strcasecmp(extension, ".com") !=0) 
		{
			if(strcasecmp(extension, ".exe") !=0) return false;
		}
		/* Run the .exe or .com file from the shell */
		/* Allocate some stack space for tables in physical memory */
		reg_sp-=0x200;
		//Add Parameter block
		DOS_ParamBlock block(SegPhys(ss)+reg_sp);
		block.Clear();
		//Add a filename
		RealPt file_name=RealMakeSeg(ss,reg_sp+0x20);
		MEM_BlockWrite(Real2Phys(file_name),fullname,(Bitu)(strlen(fullname)+1));

		/* HACK: Store full commandline for mount and imgmount */
		full_arguments.assign(line);

		/* Fill the command line */
		CommandTail cmdtail;
		cmdtail.count = 0;
        memset(&cmdtail.buffer,0,CTBUF); //Else some part of the string is uninitialized (valgrind)
        if (strlen(line)>=CTBUF) line[CTBUF-1]=0;
		cmdtail.count=(uint8_t)strlen(line);
		memcpy(cmdtail.buffer,line,strlen(line));
		cmdtail.buffer[strlen(line)]=0xd;
		/* Copy command line in stack block too */
		MEM_BlockWrite(SegPhys(ss)+reg_sp+0x100,&cmdtail,CTBUF+1);
		
		/* Split input line up into parameters, using a few special rules, most notable the one for /AAA => A\0AA
		 * Qbix: It is extremely messy, but this was the only way I could get things like /:aa and :/aa to work correctly */
		
		//Prepare string first
		char parseline[258] = { 0 };
		for(char *pl = line,*q = parseline; *pl ;pl++,q++) {
			if (*pl == '=' || *pl == ';' || *pl ==',' || *pl == '\t' || *pl == ' ') *q = 0; else *q = *pl; //Replace command separators with 0.
		} //No end of string \0 needed as parseline is larger than line

		for(char* p = parseline; (p-parseline) < 250 ;p++) { //Stay relaxed within boundaries as we have plenty of room
			if (*p == '/') { //Transform /Hello into H\0ello
				*p = 0;
				p++;
				while ( *p == 0 && (p-parseline) < 250) p++; //Skip empty fields
				if ((p-parseline) < 250) { //Found something. Lets get the first letter and break it up
					p++;
					memmove(static_cast<void*>(p + 1),static_cast<void*>(p),(250u-(unsigned int)(p-parseline)));
					if ((p-parseline) < 250) *p = 0;
				}
			}
		}
		parseline[255] = parseline[256] = parseline[257] = 0; //Just to be safe.

		/* Parse FCB (first two parameters) and put them into the current DOS_PSP */
		uint8_t add;
		uint16_t skip = 0;
		//find first argument, we end up at parseline[256] if there is only one argument (similar for the second), which exists and is 0.
		while(skip < 256 && parseline[skip] == 0) skip++;
		FCB_Parsename(dos.psp(),0x5C,0x01,parseline + skip,&add);
		skip += add;
		
		//Move to next argument if it exists
		while(parseline[skip] != 0) skip++;  //This is safe as there is always a 0 in parseline at the end.
		while(skip < 256 && parseline[skip] == 0) skip++; //Which is higher than 256
		FCB_Parsename(dos.psp(),0x6C,0x01,parseline + skip,&add);

		block.exec.fcb1=RealMake(dos.psp(),0x5C);
		block.exec.fcb2=RealMake(dos.psp(),0x6C);
		/* Set the command line in the block and save it */
		block.exec.cmdtail=RealMakeSeg(ss,reg_sp+0x100);
		block.SaveData();
#if 0
		/* Save CS:IP to some point where i can return them from */
		uint32_t oldeip=reg_eip;
		uint16_t oldcs=SegValue(cs);
		RealPt newcsip=CALLBACK_RealPointer(call_shellstop);
		SegSet16(cs,RealSeg(newcsip));
		reg_ip=RealOff(newcsip);
#endif
		packerr=false;
		/* Start up a dos execute interrupt */
		reg_ax=0x4b00;
		//Filename pointer
		SegSet16(ds,SegValue(ss));
		reg_dx=RealOff(file_name);
		//Paramblock
		SegSet16(es,SegValue(ss));
		reg_bx=reg_sp;
		SETFLAGBIT(IF,false);
		CALLBACK_RunRealInt(0x21);
		/* Restore CS:IP and the stack */
		reg_sp+=0x200;
#if 0
		reg_eip=oldeip;
		SegSet16(cs,oldcs);
#endif
		if (packerr&&infix<0&&sec->Get_bool("autoa20fix")) {
			LOG(LOG_DOSMISC,LOG_DEBUG)("Attempting autoa20fix workaround for EXEPACK error");
			if (autofixwarn==1||autofixwarn==3) WriteOut("\r\n\033[41;1m\033[1;37;1mDOSBox-X\033[0m Failed to load the executable\r\n\033[41;1m\033[37;1mDOSBox-X\033[0m Now try again with A20 fix...\r\n");
			infix=0;
			dos_a20_disable_on_exec=true;
			Execute(name, args);
			dos_a20_disable_on_exec=false;
			infix=-1;
		} else if (packerr&&infix<1&&sec->Get_bool("autoloadfix")) {
			uint16_t segment;
			uint16_t blocks = (uint16_t)(1); /* start with one paragraph, resize up later. see if it comes up below the 64KB mark */
			if (DOS_AllocateMemory(&segment,&blocks)) {
				DOS_MCB mcb((uint16_t)(segment-1));
				if (segment < 0x1000) {
					uint16_t needed = 0x1000 - segment;
					DOS_ResizeMemory(segment,&needed);
                }
                mcb.SetPSPSeg(0x40); /* FIXME: Wouldn't 0x08, a magic value used to show ownership by MS-DOS, be more appropriate here? */
                LOG(LOG_DOSMISC,LOG_DEBUG)("Attempting autoloadfix workaround for EXEPACK error");
                if (autofixwarn==2||autofixwarn==3) WriteOut("\r\n\033[41;1m\033[1;37;1mDOSBox-X\033[0m Failed to load the executable\r\n\033[41;1m\033[37;1mDOSBox-X\033[0m Now try again with LOADFIX...\r\n");
                infix=1;
                Execute(name, args);
                infix=-1;
				DOS_FreeMemory(segment);
			}
		} else if (packerr&&infix<2&&!autofixwarn) {
            WriteOut("Packed file is corrupt");
        }
		packerr=false;
	}
	return true; //Executable started
}


static const char * bat_ext=".BAT";
static const char * com_ext=".COM";
static const char * exe_ext=".EXE";
static char which_ret[DOS_PATHLENGTH+4], s_ret[DOS_PATHLENGTH+4];
extern bool wild_match(const char *haystack, char *needle);
static std::string assocs = "";
std::string DOS_Shell::hasAssociation(const char* name) {
    if (noassoc) {
        assocs = "";
        return assocs;
    }
    std::string extension = strrchr(name, '.') ? strrchr(name, '.') : ".";
    cmd_assoc_map_t::iterator iter = cmd_assoc.find(extension.c_str());
    if (iter != cmd_assoc.end()) {
        assocs = strcasecmp(extension.c_str(), iter->second.c_str()) ? iter->second : "";
        return assocs;
    }
    std::transform(extension.begin(), extension.end(), extension.begin(), ::toupper);
    for (cmd_assoc_map_t::iterator iter = cmd_assoc.begin(); iter != cmd_assoc.end(); ++iter) {
        std::string ext = iter->first;
        if (ext.find('*')==std::string::npos && ext.find('?')==std::string::npos) continue;
        std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
        if (wild_match(extension.c_str(), (char *)ext.c_str())) {
            assocs = strcasecmp(extension.c_str(), iter->second.c_str()) ? iter->second : "";
            return assocs;
        }
    }
    assocs = "";
    return assocs;
}

bool DOS_Shell::hasExecutableExtension(const char* name) {
    auto extension = strrchr(name, '.');
    if (!extension) return false;
    return (!strcasecmp(extension, com_ext) || !strcasecmp(extension, exe_ext) || !strcasecmp(extension, bat_ext));
}

char * DOS_Shell::Which(char * name) {
	size_t name_len = strlen(name);
	if(name_len >= DOS_PATHLENGTH) return nullptr;

	/* Parse through the Path to find the correct entry */
	/* Check if name is already ok but just misses an extension */

    std::string upname = name;
    std::transform(upname.begin(), upname.end(), upname.begin(), ::toupper);
	if (hasAssociation(name).size() && DOS_FileExists(name))
		return name;
	else if (hasAssociation(name).size() && DOS_FileExists(upname.c_str())) {
		strcpy(name, upname.c_str());
		return name;
	} else if (hasExecutableExtension(name)) {
		if (DOS_FileExists(name)) return name;
		strcpy(name, upname.c_str());
		if (DOS_FileExists(name)) return name;
	} else {
		/* try to find .com .exe .bat */
		strcpy(which_ret,name);
		strcat(which_ret,com_ext);
		if (DOS_FileExists(which_ret)) return which_ret;
		strcpy(which_ret,name);
		strcat(which_ret,exe_ext);
		if (DOS_FileExists(which_ret)) return which_ret;
		strcpy(which_ret,name);
		strcat(which_ret,bat_ext);
		if (DOS_FileExists(which_ret)) return which_ret;
	}

	/* No Path in filename look through path environment string */
	char path[DOS_PATHLENGTH];std::string temp;
	if (!GetEnvStr("PATH",temp)) return nullptr;
	const char * pathenv=temp.c_str();
	if (!pathenv) return nullptr;
	pathenv = strchr(pathenv,'=');
	if (!pathenv) return nullptr;
	pathenv++;
	while (*pathenv) {
		/* remove ; and ;; at the beginning. (and from the second entry etc) */
		while(*pathenv == ';')
			pathenv++;

		/* get next entry */
		Bitu i_path = 0; /* reset writer */
		while(*pathenv && (*pathenv !=';') && (i_path < DOS_PATHLENGTH) )
			path[i_path++] = *pathenv++;

		if(i_path == DOS_PATHLENGTH) {
			/* If max size. move till next ; and terminate path */
			while(*pathenv && (*pathenv != ';')) 
				pathenv++;
			path[DOS_PATHLENGTH - 1] = 0;
		} else path[i_path] = 0;

		int k=0;
		for (int i=0;i<(int)strlen(path);i++)
			if (path[i]!='\"')
				path[k++]=path[i];
		path[k]=0;

		/* check entry */
		if(size_t len = strlen(path)){
			if(len >= (DOS_PATHLENGTH - 2)) continue;

			if (uselfn&&len>3) {
				if (path[len - 1]=='\\') path[len - 1]=0;
				if (DOS_GetSFNPath(("\""+std::string(path)+"\"").c_str(), s_ret, false))
					strcpy(path, s_ret);
				len = strlen(path);
			}

			if(path[len - 1] != '\\') {
				strcat(path,"\\"); 
				len++;
			}

			//If name too long =>next
			if((name_len + len + 1) >= DOS_PATHLENGTH) continue;
			strcat(path,strchr(name, ' ')?("\""+std::string(name)+"\"").c_str():name);

			if (hasAssociation(path).size() && DOS_FileExists(path)) {
				strcpy(which_ret,path);
				return strchr(which_ret, '\"')&&DOS_GetSFNPath(which_ret, s_ret, false)?s_ret:which_ret;
			} else if (hasExecutableExtension(path)) {
				strcpy(which_ret,path);
				if (DOS_FileExists(which_ret)) return strchr(which_ret, '\"')&&DOS_GetSFNPath(which_ret, s_ret, false)?s_ret:which_ret;
			} else {
				strcpy(which_ret,path);
				strcat(which_ret,com_ext);
				if (DOS_FileExists(which_ret)) return strchr(which_ret, '\"')&&DOS_GetSFNPath(which_ret, s_ret, false)?s_ret:which_ret;
				strcpy(which_ret,path);
				strcat(which_ret,exe_ext);
				if (DOS_FileExists(which_ret)) return strchr(which_ret, '\"')&&DOS_GetSFNPath(which_ret, s_ret, false)?s_ret:which_ret;
				strcpy(which_ret,path);
				strcat(which_ret,bat_ext);
				if (DOS_FileExists(which_ret)) return strchr(which_ret, '\"')&&DOS_GetSFNPath(which_ret, s_ret, false)?s_ret:which_ret;
			}
		}
	}
	return nullptr;
}
