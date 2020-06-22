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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm> //std::copy
#include <iterator>  //std::front_inserter
#include "shell.h"
#include "timer.h"
#include "bios.h"
#include "control.h"
#include "regs.h"
#include "callback.h"
#include "support.h"
#include "inout.h"
#include "../ints/int10.h"
#include "../dos/drives.h"
#ifdef WIN32
#include "../dos/cdrom.h"
#endif

#ifdef _MSC_VER
# define MIN(a,b) ((a) < (b) ? (a) : (b))
# define MAX(a,b) ((a) > (b) ? (a) : (b))
#else
# define MIN(a,b) std::min(a,b)
# define MAX(a,b) std::max(a,b)
#endif

extern int lfn_filefind_handle;

void DOS_Shell::ShowPrompt(void) {
	char dir[DOS_PATHLENGTH];
	dir[0] = 0; //DOS_GetCurrentDir doesn't always return something. (if drive is messed up)
	DOS_GetCurrentDir(0,dir,uselfn);
	std::string line;
	const char * promptstr = "\0";

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
			case 'P': WriteOut("%c:\\%s",DOS_GetDefaultDrive()+'A',dir); break;
			case 'Q': WriteOut("="); break;
			case 'S': WriteOut(" "); break;
			case 'T': {
				Bitu ticks=(Bitu)(((65536.0 * 100.0)/(double)PIT_TICK_RATE)* mem_readd(BIOS_TIMER));
				reg_dl=(Bit8u)((Bitu)ticks % 100);
				ticks/=100;
				reg_dh=(Bit8u)((Bitu)ticks % 60);
				ticks/=60;
				reg_cl=(Bit8u)((Bitu)ticks % 60);
				ticks/=60;
				reg_ch=(Bit8u)((Bitu)ticks % 24);
				WriteOut("%2d:%02d:%02d.%02d",reg_ch,reg_cl,reg_dh,reg_dl);
				break;
			}
			case 'V': WriteOut("DOSBox version %s. Reported DOS version %d.%d.",VERSION,dos.version.major,dos.version.minor); break;
			case '$': WriteOut("$"); break;
			case '_': WriteOut("\n"); break;
			case 'M': break;
			case '+': break;
		}
		promptstr++;
	}
}

static void outc(Bit8u c) {
	Bit16u n=1;
	DOS_WriteFile(STDOUT,&c,&n);
}

static void backone() {
	BIOS_NCOLS;
	Bit8u page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
	if (CURSOR_POS_COL(page)>0)
		outc(8);
	else if (CURSOR_POS_ROW(page)>0)
		INT10_SetCursorPos(CURSOR_POS_ROW(page)-1, ncols-1, page);
}

//! \brief Moves the caret to prev row/last column when column is 0 (video mode 0).
void MoveCaretBackwards()
{
	Bit8u col, row;
	const Bit8u page(0);
	INT10_GetCursorPos(&row, &col, page);

	if (col != 0) 
		return;

	Bit16u cols;
	INT10_GetScreenColumns(&cols);
	INT10_SetCursorPos(row - 1, static_cast<Bit8u>(cols), page);
}

/* NTS: buffer pointed to by "line" must be at least CMD_MAXLINE+1 large */
void DOS_Shell::InputCommand(char * line) {
	Bitu size=CMD_MAXLINE-2; //lastcharacter+0
	Bit8u c;Bit16u n=1;
	Bit16u str_len=0;Bit16u str_index=0;
	Bit16u len=0;
	bool current_hist=false; // current command stored in history?
    Bit16u cr;

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

        if (input_handle != STDIN) { /* FIXME: Need DOS_IsATTY() or somesuch */
            cr = (Bit16u)c; /* we're not reading from the console */
        }
        else if (IS_PC98_ARCH) {
            extern Bit16u last_int16_code;

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
                    cr = (Bit16u)c;
                }
            }
            else {
                cr = (Bit16u)c;
            }
        }
        else {
            if (c == 0) {
				DOS_ReadFile(input_handle,&c,&n);
                cr = (Bit16u)c << (Bit16u)8;
            }
            else {
                cr = (Bit16u)c;
            }
        }

        switch (cr) {
            case 0x3d00:	/* F3 */
                if (!l_history.size()) break;
                it_history = l_history.begin();
                if (it_history != l_history.end() && it_history->length() > str_len) {
                    const char *reader = &(it_history->c_str())[str_len];
                    while ((c = (Bit8u)(*reader++))) {
                        line[str_index ++] = (char)c;
                        DOS_WriteFile(STDOUT,&c,&n);
                    }
                    str_len = str_index = (Bit16u)it_history->length();
                    size = (unsigned int)CMD_MAXLINE - str_index - 2u;
                    line[str_len] = 0;
                }
                break;

            case 0x4B00:	/* LEFT */
                if (str_index) {
                    backone();
                    str_index --;
                	MoveCaretBackwards();
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
						outc(static_cast<Bit8u>(line[str_index++]));
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
                if (str_index < str_len) {
                    outc((Bit8u)line[str_index++]);
                }
                break;

            case 0x4700:	/* HOME */
                while (str_index) {
                    backone();
                    str_index--;
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
                while (str_index < str_len) {
                    outc((Bit8u)line[str_index++]);
                }
                break;

            case 0x4800:	/* UP */
                if (l_history.empty() || it_history == l_history.end()) break;

                // store current command in history if we are at beginning
                if (it_history == l_history.begin() && !current_hist) {
                    current_hist=true;
                    l_history.push_front(line);
                }

                // ensure we're at end to handle all cases
                while (str_index < str_len) {
                    outc((Bit8u)line[str_index++]);
                }

                for (;str_index>0; str_index--) {
                    // removes all characters
                    backone(); outc(' '); backone();
                }
                strcpy(line, it_history->c_str());
                len = (Bit16u)it_history->length();
                str_len = str_index = len;
                size = (unsigned int)CMD_MAXLINE - str_index - 2u;
                DOS_WriteFile(STDOUT, (Bit8u *)line, &len);
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
                    outc((Bit8u)line[str_index++]);
                }

                for (;str_index>0; str_index--) {
                    // removes all characters
                    backone(); outc(' '); backone();
                }
                strcpy(line, it_history->c_str());
                len = (Bit16u)it_history->length();
                str_len = str_index = len;
                size = (unsigned int)CMD_MAXLINE - str_index - 2u;
                DOS_WriteFile(STDOUT, (Bit8u *)line, &len);
                ++it_history;

                break;
            case 0x5300:/* DELETE */
                {
                    if(str_index>=str_len) break;
                    Bit16u a=str_len-str_index-1;
                    Bit8u* text=reinterpret_cast<Bit8u*>(&line[str_index+1]);
                    DOS_WriteFile(STDOUT,text,&a);//write buffer to screen
                    outc(' ');backone();
                    for(Bitu i=str_index;i<(str_len-1u);i++) {
                        line[i]=line[i+1u];
                        backone();
                    }
                    line[--str_len]=0;
                    size++;
                }
                break;
            case 0x0F00:	/* Shift-Tab */
                if (l_completion.size()) {
                    if (it_completion == l_completion.begin()) it_completion = l_completion.end (); 
                    --it_completion;

                    if (it_completion->length()) {
                        for (;str_index > completion_index; str_index--) {
                            // removes all characters
                            backone(); outc(' '); backone();
                        }

                        strcpy(&line[completion_index], it_completion->c_str());
                        len = (Bit16u)it_completion->length();
                        str_len = str_index = (Bitu)(completion_index + len);
                        size = (unsigned int)CMD_MAXLINE - str_index - 2u;
                        DOS_WriteFile(STDOUT, (Bit8u *)it_completion->c_str(), &len);
                    }
                }
                break;
            case 0x08:				/* BackSpace */
                if (str_index) {
                    backone();
                    Bit32u str_remain=(Bit32u)(str_len - str_index);
                    size++;
                    if (str_remain) {
                        memmove(&line[str_index-1],&line[str_index],str_remain);
                        line[--str_len]=0;
                        str_index --;
                        /* Go back to redraw */
                        for (Bit16u i=str_index; i < str_len; i++)
                            outc((Bit8u)line[i]);
                    } else {
                        line[--str_index] = '\0';
                        str_len--;
                    }
                    outc(' ');	backone();
                    // moves the cursor left
                    while (str_remain--) backone();
                }
                if (l_completion.size()) l_completion.clear();
                break;
            case 0x0a:				/* Give a new Line */
                outc('\n');
                break;
            case '': // FAKE CTRL-C
                outc(94); outc('C');
                *line = 0;      // reset the line.
                if (l_completion.size()) l_completion.clear(); //reset the completion list.
                if(!echo) outc('\n');
                size = 0;       // stop the next loop
                str_len = 0;    // prevent multiple adds of the same line
                break;
            case 0x0d:				/* Don't care, and return */
                if(!echo) { outc('\r'); outc('\n'); }
                size=0;			//Kill the while loop
                break;
            case'\t':
                {
                    if (l_completion.size()) {
                        ++it_completion;
                        if (it_completion == l_completion.end()) it_completion = l_completion.begin();
                    } else {
                        // build new completion list
                        // Lines starting with CD/MD/RD will only get directories in the list
						bool dir_only = (strncasecmp(line,"CD ",3)==0)||(strncasecmp(line,"MD ",3)==0)||(strncasecmp(line,"RD ",3)==0)||
								(strncasecmp(line,"CHDIR ",6)==0)||(strncasecmp(line,"MKDIR ",3)==0)||(strncasecmp(line,"RMDIR ",6)==0);
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
							const char *sp = strrchr(line, c[j]);
							while (sp) {
								q=0;
								char *i;
								for (i=line;i<sp;i++)
									if (*i=='\"') q++;
								if (q/2*2==q) break;
								*i=0;
								sp = strrchr(line, c[j]);
								*i=c[j];
							}
							if (!p_completion_start || p_completion_start<sp)
								p_completion_start = sp;
						}

                        if (p_completion_start) {
                            p_completion_start ++;
                            completion_index = (Bit16u)(str_len - strlen(p_completion_start));
                        } else {
                            p_completion_start = line;
                            completion_index = 0;
                        }
						k=completion_index;

                        const char *path;
						if ((path = strrchr(line+completion_index,':'))) completion_index = (Bit16u)(path-line+1);
                        if ((path = strrchr(line+completion_index,'\\'))) completion_index = (Bit16u)(path-line+1);
                        if ((path = strrchr(line+completion_index,'/'))) completion_index = (Bit16u)(path-line+1);

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
                            break;
                        }
                        if (p_completion_start) {
                            safe_strncpy(mask, p_completion_start,DOS_PATHLENGTH);
                            const char* dot_pos = strrchr(mask, '.');
                            const char* bs_pos = strrchr(mask, '\\');
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
                            break;
                        }

                        DOS_DTA dta(dos.dta());
						char name[DOS_NAMELENGTH_ASCII], lname[LFN_NAMELENGTH], qlname[LFN_NAMELENGTH+2];
                        Bit32u sz;Bit16u date;Bit16u time;Bit8u att;

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
							dta.GetResult(name,lname,sz,date,time,att);
							if ((strchr(uselfn?lname:name,' ')!=NULL&&q/2*2==q)||r)
								sprintf(qlname,q/2*2!=q?"%s\"":"\"%s\"",uselfn?lname:name);
							else
                                strcpy(qlname,uselfn?lname:name);
                            // add result to completion list

                            if (strcmp(name, ".") && strcmp(name, "..")) {
                                if (dir_only) { //Handle the dir only case different (line starts with cd)
									if(att & DOS_ATTR_DIRECTORY) l_completion.push_back(qlname);
                                } else {
                                    const char *ext = strrchr(name, '.'); // file extension
                                    if (ext && (strcmp(ext, ".BAT") == 0 || strcmp(ext, ".COM") == 0 || strcmp(ext, ".EXE") == 0))
                                        // we add executables to the a seperate list and place that list infront of the normal files
                                        executable.push_front(qlname);
                                    else
										l_completion.push_back(qlname);
                                }
                            }
                            res=DOS_FindNext();
                        }
						lfn_filefind_handle=fbak;
                        /* Add executable list to front of completion list. */
                        std::copy(executable.begin(),executable.end(),std::front_inserter(l_completion));
                        it_completion = l_completion.begin();
                        dos.dta(save_dta);
                    }

                    if (l_completion.size() && it_completion->length()) {
                        for (;str_index > completion_index; str_index--) {
                            // removes all characters
                            backone(); outc(' '); backone();
                        }

                        strcpy(&line[completion_index], it_completion->c_str());
                        len = (Bit16u)it_completion->length();
                        str_len = str_index = (Bitu)(completion_index + len);
                        size = (unsigned int)CMD_MAXLINE - str_index - 2u;
                        DOS_WriteFile(STDOUT, (Bit8u *)it_completion->c_str(), &len);
                    }
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
                    outc(' ');
                    str_index++;
                }
                while (str_index > 0) {
                    backone();
                    outc(' ');
                    backone();
                    MoveCaretBackwards();
                    str_index--;
                }

                *line = 0;      // reset the line.
                if (l_completion.size()) l_completion.clear(); //reset the completion list.
                str_index = 0;
                str_len = 0;
                break;
            default:
                if (cr >= 0x100) break;
                if (l_completion.size()) l_completion.clear();
                if(str_index < str_len && !INT10_GetInsertState()) { //mem_readb(BIOS_KEYBOARD_FLAGS1)&0x80) dev_con.h ?
                    outc(' ');//move cursor one to the right.
                    Bit16u a = str_len - str_index;
                    Bit8u* text=reinterpret_cast<Bit8u*>(&line[str_index]);
                    DOS_WriteFile(STDOUT,text,&a);//write buffer to screen
                    backone();//undo the cursor the right.
                    for(Bitu i=str_len;i>str_index;i--) {
                        line[i]=line[i-1]; //move internal buffer
                        backone(); //move cursor back (from write buffer to screen)
                    }
                    line[++str_len]=0;//new end (as the internal buffer moved one place to the right
                    size--;
                }

                line[str_index]=(char)(cr&0xFF);
                str_index ++;
                if (str_index > str_len){ 
                    line[str_index] = '\0';
                    str_len++;
                    size--;
                }
                DOS_WriteFile(STDOUT,&c,&n);
                break;
        }
    }

	if (!str_len) return;
	str_len++;

	// remove current command from history if it's there
	if (current_hist) {
		current_hist=false;
		l_history.pop_front();
	}

	// add command line to history. Win95 behavior with DOSKey suggests
	// that the original string is preserved, not the expanded string.
	l_history.push_front(line); it_history = l_history.begin();
	if (l_completion.size()) l_completion.clear();

	/* DOS %variable% substitution */
	ProcessCmdLineEnvVarStitution(line);
}


/* WARNING: Substitution is carried out in-place!
 * Buffer pointed to by "line" must be at least CMD_MAXLINE+1 bytes long! */
void DOS_Shell::ProcessCmdLineEnvVarStitution(char *line) {
	char temp[CMD_MAXLINE]; /* <- NTS: Currently 4096 which is very generous indeed! */
    char* w = temp;
    const char* wf = temp + sizeof(temp) - 1;
	char *r=line;

	/* initial scan: is there anything to substitute? */
	/* if not, then return without modifying "line" */
	while (*r != 0 && *r != '%') r++;
	if (*r != '%') return;

	/* if the incoming string is already too long, then that's a problem too! */
	if (((size_t)(r+1-line)) >= CMD_MAXLINE) {
		LOG_MSG("DOS_Shell::ProcessCmdLineEnvVarStitution WARNING incoming string to substitute is already too long!\n");
		goto overflow;
	}

	/* copy the string down up to that point */
    for (const char* c = line; c < r;) {
		assert(w < wf);
		*w++ = *c++;
	}

	/* begin substitution process */
	while (*r != 0) {
		if (*r == '%') {
			r++;
			if (*r == '%' || *r == 0) {
				/* %% or leaving a trailing % at the end (Win95 behavior) becomes a single '%' */
				if (w >= wf) goto overflow;
				*w++ = '%';
				if (*r != 0) r++;
				else break;
			}
			else {
                const char* name = r; /* store pointer, 'r' is first char of the name following '%' */
				int spaces = 0,chars = 0;

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
				if (isalpha(*r) || *r == ' ') { /* must start with a letter. space is apparently valid too. (Win95) */
					if (*r == ' ') spaces++;
					else if (isalpha(*r)) chars++;

					r++;
					while (*r != 0 && *r != '%') {
						if (*r == ' ') spaces++;
						else chars++;
						r++;
					}
				}

				/* Win95 testing:
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
				 * consitutes valid or invalid %variable% names. */
				if (*r == '%' && ((spaces > 0 && chars == 0) || (spaces == 0 && chars > 0))) {
					std::string temp2;

					/* valid name found. substitute */
					*r++ = 0; /* ASCIIZ snip */
					if (GetEnvStr(name,temp2)) {
						size_t equ_pos = temp2.find_first_of('=');
						if (equ_pos != std::string::npos) {
							const char *base = temp2.c_str();
							const char *value = base + equ_pos + 1;
							const char *fence = base + temp2.length();
							assert(value >= base && value <= fence);
							size_t len = (size_t)(fence-value);

							if ((w+len) > wf) goto overflow;
							memcpy(w,value,len);
							w += len;
						}
					}
				}
				else {
					/* nope. didn't find a valid name */

					while (*r == ' ') r++; /* skip spaces */
					name--; /* step "name" back to cover the first '%' we found */

                    for (const char* c = name; c < r;) {
						if (w >= wf) goto overflow;
						*w++ = *c++;
					}
				}
			}
		}
		else {
			if (w >= wf) goto overflow;
			*w++ = *r++;
		}
	}

	/* complete the C-string */
	assert(w <= wf);
	*w = 0;

	/* copy the string back over the buffer pointed to by line */
	{
		size_t out_len = (size_t)(w+1-temp); /* length counting the NUL too */
		assert(out_len <= CMD_MAXLINE);
		memcpy(line,temp,out_len);
	}

	/* success */
	return;
overflow:
	*line = 0; /* clear string (C-string chop with NUL) */
	WriteOut("Command input error: string expansion overflow\n");
}

std::string full_arguments = "";
bool infix=false;
extern bool packerr;
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

	/* check for a drive change */
	if (((strcmp(name + 1, ":") == 0) || (strcmp(name + 1, ":\\") == 0)) && isalpha(*name))
	{
		if (strrchr(name,'\\')) { WriteOut(MSG_Get("SHELL_EXECUTE_ILLEGAL_COMMAND"),name); return true; }
		if (!DOS_SetDrive(toupper(name[0])-'A')) {
#ifdef WIN32
            const Section_prop* sec = 0; sec = static_cast<Section_prop*>(control->GetSection("dos"));
			if(!sec->Get_bool("automount")) { WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),toupper(name[0])); return true; }
			// automount: attempt direct letter to drive map.
			int type=GetDriveType(name);
			if(type==DRIVE_FIXED && (strcasecmp(name,"C:")==0)) WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_ACCESS_WARNING_WIN"));
first_1:
			if(type==DRIVE_CDROM) WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_ACCESS_CDROM"),toupper(name[0]));
			else if(type==DRIVE_REMOVABLE && (strcasecmp(name,"A:")==0||strcasecmp(name,"B:")==0)) WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_ACCESS_FLOPPY"),toupper(name[0]));
			else if(type==DRIVE_REMOVABLE) WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_ACCESS_REMOVABLE"),toupper(name[0]));
			else if(type==DRIVE_REMOTE) WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_ACCESS_NETWORK"),toupper(name[0]));
			else if((type==DRIVE_FIXED)||(type==DRIVE_RAMDISK)) WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_ACCESS_FIXED"),toupper(name[0]));
			else { WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),toupper(name[0])); return true; }

first_2:
		Bit8u c;Bit16u n=1;
		DOS_ReadFile (STDIN,&c,&n);
		do switch (c) {
			case 'n':			case 'N':
			{
				DOS_WriteFile (STDOUT,&c, &n);
				DOS_ReadFile (STDIN,&c,&n);
				do switch (c) {
					case 0xD: WriteOut("\n\n"); WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),toupper(name[0])); return true;
					case 0x3: WriteOut("^C\n");return true;
					case 0x8: WriteOut("\b \b"); goto first_2;
				} while (DOS_ReadFile (STDIN,&c,&n));
			}
			case 'y':			case 'Y':
			{
				DOS_WriteFile (STDOUT,&c, &n);
				DOS_ReadFile (STDIN,&c,&n);
				do switch (c) {
					case 0xD: WriteOut("\n"); goto continue_1;
					case 0x3: WriteOut("^C\n");return true;
					case 0x8: WriteOut("\b \b"); goto first_2;
				} while (DOS_ReadFile (STDIN,&c,&n));
			}
			case 0x3: WriteOut("^C\n");return true;
			case 0xD: WriteOut("\n"); goto first_1;
			case '\t': case 0x08: goto first_2;
			default:
			{
				DOS_WriteFile (STDOUT,&c, &n);
				DOS_ReadFile (STDIN,&c,&n);
				do switch (c) {
					case 0xD: WriteOut("\n");goto first_1;
					case 0x3: WriteOut("^C\n");return true;
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
			
			this->ParseLine(mountstring);
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
	const char* extension = strrchr(fullname,'.');
	
	/*always disallow files without extension from being executed. */
	/*only internal commands can be run this way and they never get in this handler */
	if(extension == 0)
	{
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
        memset(&cmdtail.buffer,0,CTBUF); //Else some part of the string is unitialized (valgrind)
        if (strlen(line)>=CTBUF) line[CTBUF-1]=0;
		cmdtail.count=(Bit8u)strlen(line);
		memcpy(cmdtail.buffer,line,strlen(line));
		cmdtail.buffer[strlen(line)]=0xd;
		/* Copy command line in stack block too */
		MEM_BlockWrite(SegPhys(ss)+reg_sp+0x100,&cmdtail,CTBUF+1);
		
		/* Split input line up into parameters, using a few special rules, most notable the one for /AAA => A\0AA
		 * Qbix: It is extremly messy, but this was the only way I could get things like /:aa and :/aa to work correctly */
		
		//Prepare string first
		char parseline[258] = { 0 };
		for(char *pl = line,*q = parseline; *pl ;pl++,q++) {
			if (*pl == '=' || *pl == ';' || *pl ==',' || *pl == '\t' || *pl == ' ') *q = 0; else *q = *pl; //Replace command seperators with 0.
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
		Bit8u add;
		Bit16u skip = 0;
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
		Bit32u oldeip=reg_eip;
		Bit16u oldcs=SegValue(cs);
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
		if (packerr&&!infix) {
			Bit16u segment;
			Bit16u blocks = (Bit16u)(64*1024/16);
			if (DOS_AllocateMemory(&segment,&blocks)) {
				DOS_MCB mcb((Bit16u)(segment-1));
				mcb.SetPSPSeg(0x40);
				WriteOut("\r\nTrying to run with LOADFIX..\r\n");
				infix=true;
				Execute(name, args);
				infix=false;
				DOS_FreeMemory(segment);
			}
		}
		packerr=false;
	}
	return true; //Executable started
}




static const char * bat_ext=".BAT";
static const char * com_ext=".COM";
static const char * exe_ext=".EXE";
static char which_ret[DOS_PATHLENGTH+4], s_ret[DOS_PATHLENGTH+4];

char * DOS_Shell::Which(char * name) {
	size_t name_len = strlen(name);
	if(name_len >= DOS_PATHLENGTH) return 0;

	/* Parse through the Path to find the correct entry */
	/* Check if name is already ok but just misses an extension */

	if (DOS_FileExists(name)) return name;
	upcase(name);
	if (DOS_FileExists(name)) return name;
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


	/* No Path in filename look through path environment string */
	char path[DOS_PATHLENGTH];std::string temp;
	if (!GetEnvStr("PATH",temp)) return 0;
	const char * pathenv=temp.c_str();
	if (!pathenv) return 0;
	pathenv = strchr(pathenv,'=');
	if (!pathenv) return 0;
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

			strcpy(which_ret,path);
			if (DOS_FileExists(which_ret)) return strchr(which_ret, '\"')&&DOS_GetSFNPath(which_ret, s_ret, false)?s_ret:which_ret;
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
	return 0;
}
