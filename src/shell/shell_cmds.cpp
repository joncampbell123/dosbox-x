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
 *
 *  Heavy improvements by the DOSBox-X Team, 2011-2020
 *  DXCAPTURE, DEBUGBOX, INT2FDBG commands by joncampbell123
 *  ATTRIB, COUNTRY, FOR, LFNFOR, VERIFY, TRUENAME commands by Wengier
 *  LS command by the dosbox-staging team and Wengier
 */


#include "dosbox.h"
#include "shell.h"
#include "callback.h"
#include "regs.h"
#include "pic.h"
#include "keyboard.h"
#include "timer.h"
#include "../src/ints/int10.h"
#include <time.h>
#include <assert.h>
#include "bios.h"
#include "../dos/drives.h"
#include "support.h"
#include "control.h"
#include "paging.h"
#include <algorithm>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <vector>
#include <string>
#include "build_timestamp.h"

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

static SHELL_Cmd cmd_list[]={
{	"DIR",			0,		&DOS_Shell::CMD_DIR,		"SHELL_CMD_DIR_HELP"},
{	"CD",			0,		&DOS_Shell::CMD_CHDIR,		"SHELL_CMD_CHDIR_HELP"},
{	"ALIAS",		1,		&DOS_Shell::CMD_ALIAS,		"SHELL_CMD_ALIAS_HELP"},
{	"ATTRIB",		1,		&DOS_Shell::CMD_ATTRIB,		"SHELL_CMD_ATTRIB_HELP"},
{	"BREAK",		1,		&DOS_Shell::CMD_BREAK,		"SHELL_CMD_BREAK_HELP"},
{	"CALL",			1,		&DOS_Shell::CMD_CALL,		"SHELL_CMD_CALL_HELP"},
{	"CHDIR",		1,		&DOS_Shell::CMD_CHDIR,		"SHELL_CMD_CHDIR_HELP"},
{	"CHOICE",		1,		&DOS_Shell::CMD_CHOICE,		"SHELL_CMD_CHOICE_HELP"},
{	"CLS",			0,		&DOS_Shell::CMD_CLS,		"SHELL_CMD_CLS_HELP"},
{	"COPY",			0,		&DOS_Shell::CMD_COPY,		"SHELL_CMD_COPY_HELP"},
{	"COUNTRY",		1,		&DOS_Shell::CMD_COUNTRY,	"SHELL_CMD_COUNTRY_HELP"},
{	"CTTY",			1,		&DOS_Shell::CMD_CTTY,		"SHELL_CMD_CTTY_HELP"},
{	"DATE",			0,		&DOS_Shell::CMD_DATE,		"SHELL_CMD_DATE_HELP"},
{	"DEL",			0,		&DOS_Shell::CMD_DELETE,		"SHELL_CMD_DELETE_HELP"},
{	"ECHO",			0,		&DOS_Shell::CMD_ECHO,		"SHELL_CMD_ECHO_HELP"},
{	"ERASE",		1,		&DOS_Shell::CMD_DELETE,		"SHELL_CMD_DELETE_HELP"},
{	"EXIT",			0,		&DOS_Shell::CMD_EXIT,		"SHELL_CMD_EXIT_HELP"},	
{	"FOR",			1,		&DOS_Shell::CMD_FOR,		"SHELL_CMD_FOR_HELP"},
{	"GOTO",			1,		&DOS_Shell::CMD_GOTO,		"SHELL_CMD_GOTO_HELP"},
{	"HELP",			1,		&DOS_Shell::CMD_HELP,		"SHELL_CMD_HELP_HELP"},
{	"IF",			1,		&DOS_Shell::CMD_IF,			"SHELL_CMD_IF_HELP"},
{	"LFNFOR",		1,		&DOS_Shell::CMD_LFNFOR,		"SHELL_CMD_LFNFOR_HELP"},
{	"LH",			1,		&DOS_Shell::CMD_LOADHIGH,	"SHELL_CMD_LOADHIGH_HELP"},
{	"LOADHIGH",		1,		&DOS_Shell::CMD_LOADHIGH, 	"SHELL_CMD_LOADHIGH_HELP"},
//{   "LS",			1,		&DOS_Shell::CMD_LS,			"SHELL_CMD_LS_HELP"}, // LS as a program (Z:\LS.COM) instead of shell command
{	"MD",			0,		&DOS_Shell::CMD_MKDIR,		"SHELL_CMD_MKDIR_HELP"},
{	"MKDIR",		1,		&DOS_Shell::CMD_MKDIR,		"SHELL_CMD_MKDIR_HELP"},
{	"MORE",			1,		&DOS_Shell::CMD_MORE,		"SHELL_CMD_MORE_HELP"},
{	"PATH",			1,		&DOS_Shell::CMD_PATH,		"SHELL_CMD_PATH_HELP"},
{	"PAUSE",		1,		&DOS_Shell::CMD_PAUSE,		"SHELL_CMD_PAUSE_HELP"},
{	"PROMPT",		0,		&DOS_Shell::CMD_PROMPT,		"SHELL_CMD_PROMPT_HELP"},
{	"RD",			0,		&DOS_Shell::CMD_RMDIR,		"SHELL_CMD_RMDIR_HELP"},
{	"REM",			1,		&DOS_Shell::CMD_REM,		"SHELL_CMD_REM_HELP"},
{	"REN",			0,		&DOS_Shell::CMD_RENAME,		"SHELL_CMD_RENAME_HELP"},
{	"RENAME",		1,		&DOS_Shell::CMD_RENAME,		"SHELL_CMD_RENAME_HELP"},
{	"RMDIR",		1,		&DOS_Shell::CMD_RMDIR,		"SHELL_CMD_RMDIR_HELP"},
{	"SET",			1,		&DOS_Shell::CMD_SET,		"SHELL_CMD_SET_HELP"},
{	"SHIFT",		1,		&DOS_Shell::CMD_SHIFT,		"SHELL_CMD_SHIFT_HELP"},
{	"SUBST",		1,		&DOS_Shell::CMD_SUBST,		"SHELL_CMD_SUBST_HELP"},
{	"TIME",			0,		&DOS_Shell::CMD_TIME,		"SHELL_CMD_TIME_HELP"},
{	"TYPE",			0,		&DOS_Shell::CMD_TYPE,		"SHELL_CMD_TYPE_HELP"},
{	"VER",			0,		&DOS_Shell::CMD_VER,		"SHELL_CMD_VER_HELP"},
{	"VERIFY",		1,		&DOS_Shell::CMD_VERIFY,		"SHELL_CMD_VERIFY_HELP"},
{	"VOL",			0,		&DOS_Shell::CMD_VOL,		"SHELL_CMD_VOL_HELP"},
{	"TRUENAME",		1,		&DOS_Shell::CMD_TRUENAME,	"SHELL_CMD_TRUENAME_HELP"},
// Advanced commands specific to DOSBox-X
{	"ADDKEY",		1,		&DOS_Shell::CMD_ADDKEY,		"SHELL_CMD_ADDKEY_HELP"},
{	"DX-CAPTURE",	1,		&DOS_Shell::CMD_DXCAPTURE,  "SHELL_CMD_DXCAPTURE_HELP"},
#if C_DEBUG
// Additional commands for debugging purposes in DOSBox-X
{	"DEBUGBOX",		1,		&DOS_Shell::CMD_DEBUGBOX,	"SHELL_CMD_DEBUGBOX_HELP"},
{	"INT2FDBG",		1,		&DOS_Shell::CMD_INT2FDBG,	"SHELL_CMD_INT2FDBG_HELP"},
#endif
{0,0,0,0}
}; 

extern int enablelfn, lfn_filefind_handle;
extern bool date_host_forced, usecon, rsize;
extern unsigned long freec;
extern Bit16u countryNo;
void DOS_SetCountry(Bit16u countryNo);

/* support functions */
static char empty_char = 0;
static char* empty_string = &empty_char;
static void StripSpaces(char*&args) {
	while(args && *args && isspace(*reinterpret_cast<unsigned char*>(args)))
		args++;
}

static void StripSpaces(char*&args,char also) {
	while(args && *args && (isspace(*reinterpret_cast<unsigned char*>(args)) || (*args == also)))
		args++;
}

static char* ExpandDot(char*args, char* buffer , size_t bufsize) {
	if(*args == '.') {
		if(*(args+1) == 0){
			safe_strncpy(buffer, "*.*", bufsize);
			return buffer;
		}
		if( (*(args+1) != '.') && (*(args+1) != '\\') ) {
			buffer[0] = '*';
			buffer[1] = 0;
			if (bufsize > 2) strncat(buffer,args,bufsize - 1 /*used buffer portion*/ - 1 /*trailing zero*/  );
			return buffer;
		} else
			safe_strncpy (buffer, args, bufsize);
	}
	else safe_strncpy(buffer,args, bufsize);
	return buffer;
}



bool DOS_Shell::CheckConfig(char* cmd_in,char*line) {
	bool quote=false;
	if (strlen(cmd_in)>2&&cmd_in[0]=='"'&&cmd_in[strlen(cmd_in)-1]=='"') {
		cmd_in[strlen(cmd_in)-1]=0;
		cmd_in++;
		quote=true;
	}
	Section* test = control->GetSectionFromProperty(cmd_in);
	if(!test) return false;
	if(line && !line[0]) {
		std::string val = test->GetPropValue(cmd_in);
		if(val != NO_SUCH_PROPERTY) WriteOut("%s\n",val.c_str());
		return true;
	}
	char newcom[1024]; newcom[0] = 0; strcpy(newcom,"z:\\config -set ");
	if (quote) strcat(newcom,"\"");
	strcat(newcom,test->GetName());	strcat(newcom," ");
	strcat(newcom,cmd_in);
	if (line != NULL) {
		strcat(newcom, line);
		if (quote) strcat(newcom,"\"");
	} else
		E_Exit("'line' in CheckConfig is NULL");
	DoCommand(newcom);
	return true;
}

bool enable_config_as_shell_commands = false;

void DOS_Shell::DoCommand(char * line) {
/* First split the line into command and arguments */
    char* orign_cmd_line = line;
    std::string last_alias_cmd;
    std::string altered_cmd_line;
    int alias_counter = 0;
__do_command_begin:
    if (alias_counter > 64) {
        WriteOut(MSG_Get("SHELL_EXECUTE_ALIAS_EXPAND_OVERFLOW"), orign_cmd_line);
    }
	line=trim(line);
	char cmd_buffer[CMD_MAXLINE];
	char * cmd_write=cmd_buffer;
	int q=0;
	while (*line) {
        if (strchr("/\t", *line) || (q / 2 * 2 == q && strchr(" =", *line)))
			break;
		if (*line == '"') q++;
//		if (*line == ':') break; //This breaks drive switching as that is handled at a later stage. 
		if ((*line == '.') ||(*line == '\\')) {  //allow stuff like cd.. and dir.exe cd\kees
			*cmd_write=0;
			Bit32u cmd_index=0;
			while (cmd_list[cmd_index].name) {
				if (strcasecmp(cmd_list[cmd_index].name,cmd_buffer)==0) {
					(this->*(cmd_list[cmd_index].handler))(line);
			 		return;
				}
				cmd_index++;
			}
		}
		*cmd_write++=*line++;
	}
	*cmd_write=0;
	if (strlen(cmd_buffer)==0) {
		if (strlen(line)&&line[0]=='/') WriteOut(MSG_Get("SHELL_EXECUTE_ILLEGAL_COMMAND"),line);
		return;
	}
    cmd_alias_map_t::iterator iter = cmd_alias.find(cmd_buffer);
    if (iter != cmd_alias.end() && last_alias_cmd != cmd_buffer) {
        alias_counter++;
        altered_cmd_line = iter->second + " " + line;
        line = (char*)altered_cmd_line.c_str();
        last_alias_cmd = iter->first;
        goto __do_command_begin;
    }

/* Check the internal list */
	Bit32u cmd_index=0;
	while (cmd_list[cmd_index].name) {
		if (strcasecmp(cmd_list[cmd_index].name,cmd_buffer)==0) {
			(this->*(cmd_list[cmd_index].handler))(line);
			return;
		}
		cmd_index++;
	}
/* This isn't an internal command execute it */
	char ldir[CROSS_LEN], *p=ldir;
	if (strchr(cmd_buffer,'\"')&&DOS_GetSFNPath(cmd_buffer,ldir,false)) {
		if (!strchr(cmd_buffer, '\\') && strrchr(ldir, '\\'))
			p=strrchr(ldir, '\\')+1;
		if (uselfn&&strchr(p, ' ')&&!DOS_FileExists(("\""+std::string(p)+"\"").c_str())) {
			bool append=false;
			if (DOS_FileExists(("\""+std::string(p)+".COM\"").c_str())) {append=true;strcat(p, ".COM");}
			else if (DOS_FileExists(("\""+std::string(p)+".EXE\"").c_str())) {append=true;strcat(p, ".EXE");}
			else if (DOS_FileExists(("\""+std::string(p)+".BAT\"").c_str())) {append=true;strcat(p, ".BAT");}
			if (append&&DOS_GetSFNPath(("\""+std::string(p)+"\"").c_str(), cmd_buffer,false)) if(Execute(cmd_buffer,line)) return;
		}
		if(Execute(p,line)) return;
	} else
		if(Execute(cmd_buffer,line)) return;
	if(enable_config_as_shell_commands && CheckConfig(cmd_buffer,line)) return;
	WriteOut(MSG_Get("SHELL_EXECUTE_ILLEGAL_COMMAND"),cmd_buffer);
}

#define HELP(command) \
	if (ScanCMDBool(args,"?")) { \
		WriteOut(MSG_Get("SHELL_CMD_" command "_HELP")); \
		const char* long_m = MSG_Get("SHELL_CMD_" command "_HELP_LONG"); \
		WriteOut("\n"); \
		if(strcmp("Message not Found!\n",long_m)) WriteOut(long_m); \
		else WriteOut(command "\n"); \
		return; \
	}

#if C_DEBUG
Bitu int2fdbg_hook_callback = 0;

static Bitu INT2FDBG_Handler(void) {
	if (reg_ax == 0x1605) { /* Windows init broadcast */
		int patience = 500;
		Bitu st_seg,st_ofs;

		LOG_MSG("INT 2Fh debug hook: Caught Windows init broadcast results (ES:BX=%04x:%04x DS:SI=%04x:%04x CX=%04x DX=%04x DI=%04x)\n",
			SegValue(es),reg_bx,
			SegValue(ds),reg_si,
			reg_cx,reg_dx,reg_di);

		st_seg = SegValue(es);
		st_ofs = reg_bx;
		while (st_seg != 0 || st_ofs != 0) {
			unsigned char v_major,v_minor;
			Bitu st_seg_next,st_ofs_next;
			Bitu idrc_seg,idrc_ofs;
			Bitu vdev_seg,vdev_ofs;
			Bitu name_seg,name_ofs;
			char devname[64];
			PhysPt st_o;

			if (--patience <= 0) {
				LOG_MSG("**WARNING: Chain is too long. Something might have gotten corrupted\n");
				break;
			}

			st_o = PhysMake(st_seg,st_ofs);
			/* +0x00: Major, minor version of info structure
			 * +0x02: pointer to next startup info structure or 0000:0000
			 * +0x06: pointer to ASCIIZ name of virtual device or 0000:0000
			 * +0x0A: virtual device ref data (pointer to?? or actual data??) or 0000:0000
			 * +0x0E: pointer to instance data records or 0000:0000
			 * Windows 95 or later (v4.0+):
			 * +0x12: pointer to optionally-instanced data records or 0000:0000 */
			v_major = mem_readb(st_o+0x00);
			v_minor = mem_readb(st_o+0x01);
			st_seg_next = mem_readw(st_o+0x02+2);
			st_ofs_next = mem_readw(st_o+0x02+0);
			name_ofs = mem_readw(st_o+0x06+0);
			name_seg = mem_readw(st_o+0x06+2);
			vdev_ofs = mem_readw(st_o+0x0A+0);
			vdev_seg = mem_readw(st_o+0x0A+2);
			idrc_ofs = mem_readw(st_o+0x0A+4);	/* FIXME: 0x0E+0 and 0x0E+2 generates weird compiler error WTF?? */
			idrc_seg = mem_readw(st_o+0x0A+6);

			{
				devname[0] = 0;
				if (name_seg != 0 || name_ofs != 0) {
					unsigned char c;
					unsigned int i;
					PhysPt scan;

					scan = PhysMake(name_seg,name_ofs);
					for (i=0;i < 63 && (c=mem_readb(scan++)) != 0;) devname[i++] = (char)c;
					devname[i] = 0;
				}
			}

			LOG_MSG(" >> Version %u.%u\n",v_major,v_minor);
			LOG_MSG("    Next entry at %04x:%04x\n",(int)st_seg_next,(int)st_ofs_next);
			LOG_MSG("    Virtual device name: %04x:%04x '%s'\n",(int)name_seg,(int)name_ofs,devname);
			LOG_MSG("    Virtual dev ref data: %04x:%04x\n",(int)vdev_seg,(int)vdev_ofs);
			LOG_MSG("    Instance data records: %04x:%04x\n",(int)idrc_seg,(int)idrc_ofs);

			st_seg = st_seg_next;
			st_ofs = st_ofs_next;
		}

		LOG_MSG("----END CHAIN\n");
	}

	return CBRET_NONE;
}

/* NTS: I know I could just modify the DOS kernel's INT 2Fh code to receive the init call,
 *      the problem is that at that point, the registers do not yet contain anything interesting.
 *      all the interesting results of the call are added by TSRs on the way back UP the call
 *      chain. The purpose of this program therefore is to hook INT 2Fh on the other end
 *      of the call chain so that we can see the results just before returning INT 2Fh back
 *      to WIN.COM */
void DOS_Shell::CMD_INT2FDBG(char * args) {
	HELP("INT2FDBG");
    while (*args == ' ') args++;
    if (!strcmp(args,"-?")) {
		args[0]='/';
		HELP("INT2FDBG");
		return;
	}

	/* TODO: Allow /U to remove INT 2Fh hook */
	if (ScanCMDBool(args,"I")) {
		if (int2fdbg_hook_callback == 0) {
			Bit32u old_int2Fh;
			PhysPt w;

			int2fdbg_hook_callback = CALLBACK_Allocate();
			CALLBACK_Setup(int2fdbg_hook_callback,&INT2FDBG_Handler,CB_IRET,"INT 2Fh DBG callback");

			/* record old vector, set our new vector */
			old_int2Fh = RealGetVec(0x2f);
			w = CALLBACK_PhysPointer(int2fdbg_hook_callback);
			RealSetVec(0x2f,CALLBACK_RealPointer(int2fdbg_hook_callback));

			/* overwrite the callback with code to chain the call down, then invoke our callback on the way back up: */

			/* first, chain to the previous INT 15h handler */
			phys_writeb(w++,(Bit8u)0x9C);					//PUSHF
			phys_writeb(w++,(Bit8u)0x9A);					//CALL FAR <address>
			phys_writew(w,(Bit16u)(old_int2Fh&0xFFFF)); w += 2;		//offset
			phys_writew(w,(Bit16u)((old_int2Fh>>16)&0xFFFF)); w += 2;	//seg

			/* then, having returned from it, invoke our callback */
			phys_writeb(w++,(Bit8u)0xFE);					//GRP 4
			phys_writeb(w++,(Bit8u)0x38);					//Extra Callback instruction
			phys_writew(w,(Bit16u)int2fdbg_hook_callback); w += 2;		//The immediate word

			/* return */
			phys_writeb(w++,(Bit8u)0xCF);					//IRET

			LOG_MSG("INT 2Fh debugging hook set\n");
			WriteOut("INT 2Fh hook set\n");
		}
		else {
			WriteOut("INT 2Fh hook already setup\n");
		}
	}
	else if (*args)
		WriteOut("Invalid parameter - %s\n", args);
	else
		WriteOut("%s\n%s", MSG_Get("SHELL_CMD_INT2FDBG_HELP"), MSG_Get("SHELL_CMD_INT2FDBG_HELP_LONG"));
}
#endif

void DOS_Shell::CMD_BREAK(char * args) {
	HELP("BREAK");
	args = trim(args);
	if (!*args)
		WriteOut("BREAK is %s\n", dos.breakcheck ? "on" : "off");
	else if (!strcasecmp(args, "OFF"))
		dos.breakcheck = false;
	else if (!strcasecmp(args, "ON"))
		dos.breakcheck = true;
	else
		WriteOut("Must specify ON or OFF\n");
}

void DOS_Shell::CMD_CLS(char * args) {
	HELP("CLS");
   if (CurMode->type==M_TEXT || IS_PC98_ARCH)
       WriteOut("[2J");
   else { 
      reg_ax=(Bit16u)CurMode->mode; 
      CALLBACK_RunRealInt(0x10); 
   } 
}

void DOS_Shell::CMD_DELETE(char * args) {
	HELP("DELETE");
	bool optP=ScanCMDBool(args,"P");
	bool optF=ScanCMDBool(args,"F");
	bool optQ=ScanCMDBool(args,"Q");

	// ignore /f, /s, /ar, /as, /ah and /aa switches for compatibility
	ScanCMDBool(args,"S");
	ScanCMDBool(args,"AR");
	ScanCMDBool(args,"AS");
	ScanCMDBool(args,"AH");
	ScanCMDBool(args,"AA");

	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	if (!*args) {
		WriteOut(MSG_Get("SHELL_MISSING_PARAMETER"));
		return;
	}

	StripSpaces(args);
	args=trim(args);

	/* Command uses dta so set it to our internal dta */
	//DOS_DTA dta(dos.dta());
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	/* If delete accept switches mind the space in front of them. See the dir /p code */ 

	char full[DOS_PATHLENGTH],sfull[DOS_PATHLENGTH+2];
	char buffer[CROSS_LEN];
    char name[DOS_NAMELENGTH_ASCII],lname[LFN_NAMELENGTH+1];
    Bit32u size;Bit16u time,date;Bit8u attr;
	args = ExpandDot(args,buffer, CROSS_LEN);
	StripSpaces(args);
	if (!DOS_Canonicalize(args,full)) { WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));dos.dta(save_dta);return; }
	if (strlen(args)&&args[strlen(args)-1]!='\\') {
		Bit16u fattr;
		if (strcmp(args,"*.*")&&DOS_GetFileAttr(args, &fattr) && (fattr&DOS_ATTR_DIRECTORY))
			strcat(args, "\\");
	}
	if (strlen(args)&&args[strlen(args)-1]=='\\') strcat(args, "*.*");
	else if (!strcmp(args,".")||(strlen(args)>1&&(args[strlen(args)-2]==':'||args[strlen(args)-2]=='\\')&&args[strlen(args)-1]=='.')) {
		args[strlen(args)-1]='*';
		strcat(args, ".*");
	} else if (uselfn&&strchr(args, '*')) {
		char * find_last;
		find_last=strrchr(args,'\\');
		if (find_last==NULL) find_last=args;
		else find_last++;
		if (strlen(find_last)>0&&args[strlen(args)-1]=='*'&&strchr(find_last, '.')==NULL) strcat(args, ".*");
	}
	if (!strcmp(args,"*.*")||(strlen(args)>3&&(!strcmp(args+strlen(args)-4, "\\*.*") || !strcmp(args+strlen(args)-4, ":*.*")))) {
		if (!optQ) {
first_1:
			WriteOut(MSG_Get("SHELL_CMD_DEL_SURE"));
first_2:
			Bit8u c;Bit16u n=1;
			DOS_ReadFile (STDIN,&c,&n);
			do switch (c) {
			case 'n':			case 'N':
			{
				DOS_WriteFile (STDOUT,&c, &n);
				DOS_ReadFile (STDIN,&c,&n);
				do switch (c) {
					case 0xD: WriteOut("\n");dos.dta(save_dta);return;
					case 0x03: WriteOut("^C\n");dos.dta(save_dta);return;
					case 0x08: WriteOut("\b \b"); goto first_2;
				} while (DOS_ReadFile (STDIN,&c,&n));
			}
			case 'y':			case 'Y':
			{
				DOS_WriteFile (STDOUT,&c, &n);
				DOS_ReadFile (STDIN,&c,&n);
				do switch (c) {
					case 0xD: WriteOut("\n"); goto continue_1;
					case 0x03: WriteOut("^C\n");dos.dta(save_dta);return;
					case 0x08: WriteOut("\b \b"); goto first_2;
				} while (DOS_ReadFile (STDIN,&c,&n));
			}
			case 0xD: WriteOut("\n"); goto first_1;
			case 0x03: WriteOut("^C\n");dos.dta(save_dta);return;
			case '\t':
			case 0x08:
				goto first_2;
			default:
			{
				DOS_WriteFile (STDOUT,&c, &n);
				DOS_ReadFile (STDIN,&c,&n);
				do switch (c) {
					case 0xD: WriteOut("\n"); goto first_1;
					case 0x03: WriteOut("^C\n");dos.dta(save_dta);return;
					case 0x08: WriteOut("\b \b"); goto first_2;
				} while (DOS_ReadFile (STDIN,&c,&n));
				goto first_2;
			}
		} while (DOS_ReadFile (STDIN,&c,&n));
	}
}

continue_1:
	/* Command uses dta so set it to our internal dta */
	if (!DOS_Canonicalize(args,full)) { WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));dos.dta(save_dta);return; }
	char path[DOS_PATHLENGTH], spath[DOS_PATHLENGTH], pattern[DOS_PATHLENGTH], *r=strrchr(full, '\\');
	if (r!=NULL) {
		*r=0;
		strcpy(path, full);
		strcat(path, "\\");
		strcpy(pattern, r+1);
		*r='\\';
	} else {
		strcpy(path, "");
		strcpy(pattern, full);
	}
	int k=0;
	for (int i=0;i<(int)strlen(pattern);i++)
		if (pattern[i]!='\"')
			pattern[k++]=pattern[i];
	pattern[k]=0;
	strcpy(spath, path);
	if (strchr(args,'\"')||uselfn) {
		if (!DOS_GetSFNPath(("\""+std::string(path)+"\\").c_str(), spath, false)) strcpy(spath, path);
		if (!strlen(spath)||spath[strlen(spath)-1]!='\\') strcat(spath, "\\");
	}
	std::string pfull=std::string(spath)+std::string(pattern);
	int fbak=lfn_filefind_handle;
	lfn_filefind_handle=uselfn?LFN_FILEFIND_INTERNAL:LFN_FILEFIND_NONE;
    bool res=DOS_FindFirst((char *)((uselfn&&pfull.length()&&pfull[0]!='"'?"\"":"")+pfull+(uselfn&&pfull.length()&&pfull[pfull.length()-1]!='"'?"\"":"")).c_str(),0xffff & ~DOS_ATTR_VOLUME);
	if (!res) {
		lfn_filefind_handle=fbak;
		WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"),args);
		dos.dta(save_dta);
		return;
	}
	lfn_filefind_handle=fbak;
	//end can't be 0, but if it is we'll get a nice crash, who cares :)
	strcpy(sfull,full);
	char * end=strrchr(full,'\\')+1;*end=0;
	char * lend=strrchr(sfull,'\\')+1;*lend=0;
	dta=dos.dta();
	bool exist=false;
	lfn_filefind_handle=uselfn?LFN_FILEFIND_INTERNAL:LFN_FILEFIND_NONE;
	while (res) {
		dta.GetResult(name,lname,size,date,time,attr);
		if (!optF && (attr & DOS_ATTR_READ_ONLY) && !(attr & DOS_ATTR_DIRECTORY)) {
			exist=true;
			strcpy(end,name);
			strcpy(lend,lname);
			WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"),uselfn?sfull:full);
		} else if (!(attr & DOS_ATTR_DIRECTORY)) {
			exist=true;
			strcpy(end,name);
			strcpy(lend,lname);
			if (optP) {
				WriteOut("Delete %s (Y/N)?", uselfn?sfull:full);
				Bit8u c;
				Bit16u n=1;
				DOS_ReadFile (STDIN,&c,&n);
				if (c==3) {WriteOut("^C\r\n");break;}
				c = c=='y'||c=='Y' ? 'Y':'N';
				WriteOut("%c\r\n", c);
				if (c=='N') {lfn_filefind_handle=uselfn?LFN_FILEFIND_INTERNAL:LFN_FILEFIND_NONE;res = DOS_FindNext();continue;}
			}
			if (strlen(full)) {
				std::string pfull=(uselfn||strchr(full, ' ')?(full[0]!='"'?"\"":""):"")+std::string(full)+(uselfn||strchr(full, ' ')?(full[strlen(full)-1]!='"'?"\"":""):"");
				bool reset=false;
				if (optF && (attr & DOS_ATTR_READ_ONLY)&&DOS_SetFileAttr(pfull.c_str(), attr & ~DOS_ATTR_READ_ONLY)) reset=true;
				if (!DOS_UnlinkFile(pfull.c_str())) {
					if (optF&&reset) DOS_SetFileAttr(pfull.c_str(), attr);
					WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"),uselfn?sfull:full);
				}
			} else WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"),uselfn?sfull:full);
		}
		res=DOS_FindNext();
	}
	lfn_filefind_handle=fbak;
	if (!exist) WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
	dos.dta(save_dta);
}

static size_t GetPauseCount() {
	Bit16u rows;
	if (IS_PC98_ARCH)
		rows=real_readb(0x60,0x113) & 0x01 ? 25 : 20;
	else
		rows=real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1;
	return (rows > 3u) ? (rows - 3u) : 22u;
}

void DOS_Shell::CMD_HELP(char * args){
	HELP("HELP");
	bool optall=ScanCMDBool(args,"A")|ScanCMDBool(args,"ALL");
	/* Print the help */
	args = trim(args);
	upcase(args);
	if(!optall&&!*args) WriteOut(MSG_Get("SHELL_CMD_HELP"));
	Bit32u cmd_index=0,write_count=0;
	bool show=false;
	while (cmd_list[cmd_index].name) {
		if (optall || (*args && !strcmp(args, cmd_list[cmd_index].name)) || (!*args && !cmd_list[cmd_index].flags)) {
			show=true;
			if (*args && !strcmp(args, cmd_list[cmd_index].name) && !optall) {
				std::string cmd=std::string(args);
				if (cmd=="CD") cmd="CHDIR";
				else if (cmd=="DEL"||cmd=="ERASE") cmd="DELETE";
				else if (cmd=="LH") cmd="LOADHIGH";
				else if (cmd=="MD") cmd="MKDIR";
				else if (cmd=="RD") cmd="RMDIR";
				else if (cmd=="REN") cmd="RENAME";
				else if (cmd=="DX-CAPTURE") cmd="DXCAPTURE";
				WriteOut("%s\n%s",MSG_Get(cmd_list[cmd_index].help), MSG_Get(("SHELL_CMD_" +cmd+ "_HELP_LONG").c_str()));
			} else {
				WriteOut("<\033[34;1m%-8s\033[0m> %s",cmd_list[cmd_index].name,MSG_Get(cmd_list[cmd_index].help));
				if(!(++write_count%GetPauseCount())) {
					WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
					Bit8u c;Bit16u n=1;
					DOS_ReadFile(STDIN,&c,&n);
					if (c==3) {WriteOut("^C\r\n");break;}
					if (c==0) DOS_ReadFile(STDIN,&c,&n); // read extended key
				}
			}
		}
		cmd_index++;
	}
	if (*args&&!show) {
		std::string argc=std::string(StripArg(args));
		if (argc!=""&&argc!="CWSDPMI") DoCommand((char *)(argc+(argc=="DOS4GW"||argc=="DOS32A"?"":" /?")).c_str());
	}
}

static void removeChar(char *str, char c) {
    char *src, *dst;
    for (src = dst = str; *src != '\0'; src++) {
        *dst = *src;
        if (*dst != c) dst++;
    }
    *dst = '\0';
}

void DOS_Shell::CMD_RENAME(char * args){
	HELP("RENAME");
	StripSpaces(args);
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	if (!*args) {SyntaxError();return;}
	char * arg1=StripArg(args);
	StripSpaces(args);
	if (!*args) {SyntaxError();return;}
	char * arg2=StripArg(args);
	StripSpaces(args);
	if (*args) {SyntaxError();return;}
	char* slash = strrchr(arg1,'\\');
	Bit32u size;Bit16u date;Bit16u time;Bit8u attr;
	char name[DOS_NAMELENGTH_ASCII], lname[LFN_NAMELENGTH+1], tname1[LFN_NAMELENGTH+1], tname2[LFN_NAMELENGTH+1], text1[LFN_NAMELENGTH+1], text2[LFN_NAMELENGTH+1], tfull[CROSS_LEN+2];
	//dir_source and target are introduced for when we support multiple files being renamed.
	char sargs[CROSS_LEN], targs[CROSS_LEN], dir_source[DOS_PATHLENGTH + 4] = {0}, dir_target[CROSS_LEN + 4] = {0}, target[CROSS_LEN + 4] = {0}; //not sure if drive portion is included in pathlength

	if (!slash) slash = strrchr(arg1,':');
	if (slash) {
		/* If directory specified (crystal caves installer)
		 * rename from c:\X : rename c:\abc.exe abc.shr. 
		 * File must appear in C:\ 
		 * Ren X:\A\B C => ren X:\A\B X:\A\C */
		 
		//Copy first and then modify, makes GCC happy
		safe_strncpy(dir_source,arg1,DOS_PATHLENGTH + 4);
		char* dummy = strrchr(dir_source,'\\');
		if (!dummy) dummy = strrchr(dir_source,':');
		if (!dummy) { //Possible due to length
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			return;
		}
		dummy++;
		*dummy = 0;
		if (strchr(arg2,'\\')||strchr(arg2,':')) {
			safe_strncpy(dir_target,arg2,DOS_PATHLENGTH + 4);
			dummy = strrchr(dir_target,'\\');
			if (!dummy) dummy = strrchr(dir_target,':');
			if (dummy) {
				dummy++;
				*dummy = 0;
				if (strcasecmp(dir_source, dir_target)) {
					WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
					return;
				}
			}
			arg2=strrchr(arg2,strrchr(arg2,'\\')?'\\':':')+1;
		}
		if (strlen(dummy)&&dummy[strlen(dummy)-1]==':')
			strcat(dummy, ".\\");
	} else {
		if (strchr(arg2,'\\')||strchr(arg2,':')) {SyntaxError();return;};
		strcpy(dir_source, ".\\");
	}
	
	strcpy(target,arg2);

	char path[DOS_PATHLENGTH], spath[DOS_PATHLENGTH], pattern[DOS_PATHLENGTH], full[DOS_PATHLENGTH], *r;
	if (!DOS_Canonicalize(arg1,full)) return;
	r=strrchr(full, '\\');
	if (r!=NULL) {
		*r=0;
		strcpy(path, full);
		strcat(path, "\\");
		strcpy(pattern, r+1);
		*r='\\';
	} else {
		strcpy(path, "");
		strcpy(pattern, full);
	}
	int k=0;
	for (int i=0;i<(int)strlen(pattern);i++)
		if (pattern[i]!='\"')
			pattern[k++]=pattern[i];
	pattern[k]=0;
	strcpy(spath, path);
	if (strchr(arg1,'\"')||uselfn) {
		if (!DOS_GetSFNPath(("\""+std::string(path)+"\\").c_str(), spath, false)) strcpy(spath, path);
		if (!strlen(spath)||spath[strlen(spath)-1]!='\\') strcat(spath, "\\");
	}
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	std::string pfull=std::string(spath)+std::string(pattern);
	int fbak=lfn_filefind_handle;
	lfn_filefind_handle=uselfn?LFN_FILEFIND_INTERNAL:LFN_FILEFIND_NONE;
	if (!DOS_FindFirst((char *)((uselfn&&pfull.length()&&pfull[0]!='"'?"\"":"")+pfull+(uselfn&&pfull.length()&&pfull[pfull.length()-1]!='"'?"\"":"")).c_str(), strchr(arg1,'*')!=NULL || strchr(arg1,'?')!=NULL ? 0xffff & ~DOS_ATTR_VOLUME & ~DOS_ATTR_DIRECTORY : 0xffff & ~DOS_ATTR_VOLUME)) {
		lfn_filefind_handle=fbak;
		WriteOut(MSG_Get("SHELL_CMD_RENAME_ERROR"),arg1);
	} else {
		std::vector<std::string> sources;
		sources.clear();
	
		do {    /* File name and extension */
			dta.GetResult(name,lname,size,date,time,attr);
			lfn_filefind_handle=fbak;

			if(!(attr&DOS_ATTR_DIRECTORY && (!strcmp(name, ".") || !strcmp(name, "..")))) {
				strcpy(dir_target, target);
				removeChar(dir_target, '\"');
				arg2=dir_target;
				strcpy(sargs, dir_source);
				if (uselfn) removeChar(sargs, '\"');
				strcat(sargs, uselfn?lname:name);
				if (uselfn&&strchr(arg2,'*')&&!strchr(arg2,'.')) strcat(arg2, ".*");
				char *dot1=strrchr(uselfn?lname:name,'.'), *dot2=strrchr(arg2,'.'), *star;
				if (dot2==NULL) {
					star=strchr(arg2,'*');
					if (strchr(arg2,'?')) {
						for (unsigned int i=0; i<(uselfn?LFN_NAMELENGTH:DOS_NAMELENGTH) && i<(star?star-arg2:strlen(arg2)); i++) {
							if (*(arg2+i)=='?'&&i<strlen(name))
								*(arg2+i)=name[i];
						}
					}
					if (star) {
						if ((unsigned int)(star-arg2)<strlen(name))
							strcpy(star, name+(star-arg2));
						else
							*star=0;
					}
					removeChar(arg2, '?');
				} else {
					if (dot1) {
						*dot1=0;
						strcpy(tname1, uselfn?lname:name);
						*dot1='.';
					} else
						strcpy(tname1, uselfn?lname:name);
					*dot2=0;
					strcpy(tname2, arg2);
					*dot2='.';
					star=strchr(tname2,'*');
					if (strchr(tname2,'?')) {
						for (unsigned int i=0; i<(uselfn?LFN_NAMELENGTH:DOS_NAMELENGTH) && i<(star?star-tname2:strlen(tname2)); i++) {
							if (*(tname2+i)=='?'&&i<strlen(tname1))
								*(tname2+i)=tname1[i];
						}
					}
					if (star) {
						if ((unsigned int)(star-tname2)<strlen(tname1))
							strcpy(star, tname1+(star-tname2));
						else
							*star=0;
					}
					removeChar(tname2, '?');
					if (dot1) {
						strcpy(text1, dot1+1);
						strcpy(text2, dot2+1);
						star=strchr(text2,'*');
						if (strchr(text2,'?')) {
							for (unsigned int i=0; i<(uselfn?LFN_NAMELENGTH:DOS_NAMELENGTH) && i<(star?star-text2:strlen(text2)); i++) {
								if (*(text2+i)=='?'&&i<strlen(text1))
									*(text2+i)=text1[i];
							}
						}
						if (star) {
							if ((unsigned int)(star-text2)<strlen(text1))
								strcpy(star, text1+(star-text2));
							else
								*star=0;
						}
					} else {
						strcpy(text2, dot2+1);
						if (strchr(text2,'?')||strchr(text2,'*')) {
							for (unsigned int i=0; i<(uselfn?LFN_NAMELENGTH:DOS_NAMELENGTH) && i<(star?star-text2:strlen(text2)); i++) {
								if (*(text2+i)=='*') {
									*(text2+i)=0;
									break;
								}
							}
						}
					}
					removeChar(text2, '?');
					strcpy(tfull, tname2);
					strcat(tfull, ".");
					strcat(tfull, text2);
					arg2=tfull;
				}
				strcpy(targs, dir_source);
				if (uselfn) removeChar(targs, '\"');
				strcat(targs, arg2);
				sources.push_back(uselfn?((sargs[0]!='"'?"\"":"")+std::string(sargs)+(sargs[strlen(sargs)-1]!='"'?"\"":"")).c_str():sargs);
				sources.push_back(uselfn?((targs[0]!='"'?"\"":"")+std::string(targs)+(targs[strlen(targs)-1]!='"'?"\"":"")).c_str():targs);
				sources.push_back(strlen(sargs)>2&&sargs[0]=='.'&&sargs[1]=='\\'?sargs+2:sargs);
			}
			lfn_filefind_handle=uselfn?LFN_FILEFIND_INTERNAL:LFN_FILEFIND_NONE;
		} while ( DOS_FindNext() );
		lfn_filefind_handle=fbak;
		if (sources.empty()) WriteOut(MSG_Get("SHELL_CMD_RENAME_ERROR"),arg1);
		else {
			for (std::vector<std::string>::iterator source = sources.begin(); source != sources.end(); ++source) {
				char *oname=(char *)source->c_str();
				source++;
				if (source==sources.end()) break;
				char *nname=(char *)source->c_str();
				source++;
				if (source==sources.end()||oname==NULL||nname==NULL) break;
				char *fname=(char *)source->c_str();
				if (!DOS_Rename(oname,nname)&&fname!=NULL)
					WriteOut(MSG_Get("SHELL_CMD_RENAME_ERROR"),fname);
			}
		}
	}
	dos.dta(save_dta);
}

void DOS_Shell::CMD_ECHO(char * args){
	if (!*args) {
		if (echo) { WriteOut(MSG_Get("SHELL_CMD_ECHO_ON"));}
		else { WriteOut(MSG_Get("SHELL_CMD_ECHO_OFF"));}
		return;
	}
	char buffer[512];
	char* pbuffer = buffer;
	safe_strncpy(buffer,args,512);
	StripSpaces(pbuffer);
	if (strcasecmp(pbuffer,"OFF")==0) {
		echo=false;		
		return;
	}
	if (strcasecmp(pbuffer,"ON")==0) {
		echo=true;		
		return;
	}
	if(strcasecmp(pbuffer,"/?")==0) { HELP("ECHO"); }

	args++;//skip first character. either a slash or dot or space
	size_t len = strlen(args); //TODO check input of else ook nodig is.
	if(len && args[len - 1] == '\r') {
		LOG(LOG_MISC,LOG_WARN)("Hu ? carriage return already present. Is this possible?");
		WriteOut("%s\n",args);
	} else WriteOut("%s\r\n",args);
}


void DOS_Shell::CMD_EXIT(char * args) {
	HELP("EXIT");
	exit = true;
}

void DOS_Shell::CMD_CHDIR(char * args) {
	HELP("CHDIR");
	StripSpaces(args);
	char sargs[CROSS_LEN];
	if (*args && !DOS_GetSFNPath(args,sargs,false)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}
	Bit8u drive = DOS_GetDefaultDrive()+'A';
	char dir[DOS_PATHLENGTH];
	if (!*args) {
        DOS_GetCurrentDir(0,dir,true);
		WriteOut("%c:\\%s\n",drive,dir);
	} else if(strlen(args) == 2 && args[1]==':') {
		Bit8u targetdrive = (args[0] | 0x20)-'a' + 1;
		unsigned char targetdisplay = *reinterpret_cast<unsigned char*>(&args[0]);
        if(!DOS_GetCurrentDir(targetdrive,dir,true)) { // verify that this should be true
			if(drive == 'Z') {
				WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),toupper(targetdisplay));
			} else {
				WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			}
			return;
		}
		WriteOut("%c:\\%s\n",toupper(targetdisplay),dir);
		if(drive == 'Z')
			WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT"),toupper(targetdisplay));
	} else if (!DOS_ChangeDir(sargs)) {
		/* Changedir failed. Check if the filename is longer then 8 and/or contains spaces */
	   
		std::string temps(args),slashpart;
		std::string::size_type separator = temps.find_first_of("\\/");
		if(!separator) {
			slashpart = temps.substr(0,1);
			temps.erase(0,1);
		}
        separator = temps.find_first_of("\"");
        if(separator != std::string::npos) temps.erase(separator);
		separator = temps.rfind('.');
		if(separator != std::string::npos) temps.erase(separator);
		separator = temps.find(' ');
		if(separator != std::string::npos) {/* Contains spaces */
			temps.erase(separator);
			if(temps.size() >6) temps.erase(6);
			temps += "~1";
			WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT_2"),temps.insert(0,slashpart).c_str());
		} else {
			if (drive == 'Z') {
				WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT_3"));
			} else {
				WriteOut(MSG_Get("SHELL_CMD_CHDIR_ERROR"),args);
			}
		}
	}
}

void DOS_Shell::CMD_MKDIR(char * args) {
	HELP("MKDIR");
	StripSpaces(args);
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	if (!*args) {
		WriteOut(MSG_Get("SHELL_MISSING_PARAMETER"));
		return;
	}
	if (!DOS_MakeDir(args)) {
		WriteOut(MSG_Get("SHELL_CMD_MKDIR_ERROR"),args);
	}
}

void DOS_Shell::CMD_RMDIR(char * args) {
	HELP("RMDIR");
	// ignore /s,and /q switches for compatibility
	ScanCMDBool(args,"S");
	ScanCMDBool(args,"Q");
	StripSpaces(args);
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	if (!*args) {
		WriteOut(MSG_Get("SHELL_MISSING_PARAMETER"));
		return;
	}
	if (!DOS_RemoveDir(args)) {
		WriteOut(MSG_Get("SHELL_CMD_RMDIR_ERROR"),args);
	}
}

static void FormatNumber(Bit64u num,char * buf) {
	Bit32u numm,numk,numb,numg,numt;
	numb=num % 1000;
	num/=1000;
	numk=num % 1000;
	num/=1000;
	numm=num % 1000;
	num/=1000;
	numg=num % 1000;
	num/=1000;
	numt=num;
	if (numt) {
		sprintf(buf,"%u%c%03u%c%03u%c%03u%c%03u",numt,dos.tables.country[7],numg,dos.tables.country[7],numm,dos.tables.country[7],numk,dos.tables.country[7],numb);
		return;
	}
	if (numg) {
		sprintf(buf,"%u%c%03u%c%03u%c%03u",numg,dos.tables.country[7],numm,dos.tables.country[7],numk,dos.tables.country[7],numb);
		return;
	}
	if (numm) {
		sprintf(buf,"%u%c%03u%c%03u",numm,dos.tables.country[7],numk,dos.tables.country[7],numb);
		return;
	}
	if (numk) {
		sprintf(buf,"%u%c%03u",numk,dos.tables.country[7],numb);
		return;
	}
	sprintf(buf,"%u",numb);
}

char buffer[15] = {0};
char *FormatDate(Bit16u year, Bit8u month, Bit8u day) {
	char formatstring[6], c=dos.tables.country[11];
	sprintf(formatstring, dos.tables.country[0]==1?"D%cM%cY":(dos.tables.country[0]==2?"Y%cM%cD":"M%cD%cY"), c, c);
	Bitu bufferptr=0;
	for(Bitu i = 0; i < 5; i++) {
		if(i==1 || i==3) {
			buffer[bufferptr] = formatstring[i];
			bufferptr++;
		} else {
			if(formatstring[i]=='M') bufferptr += (Bitu)sprintf(buffer+bufferptr,"%02u", month);
			if(formatstring[i]=='D') bufferptr += (Bitu)sprintf(buffer+bufferptr,"%02u", day);
			if(formatstring[i]=='Y') bufferptr += (Bitu)sprintf(buffer+bufferptr,"%04u", year);
		}
	}
	return buffer;
}

char *FormatTime(Bitu hour, Bitu min, Bitu sec, Bitu msec)	{
	Bitu fhour=hour;
	static char retBuf[14];
	char ampm[3]="";
	if (!(dos.tables.country[17]&1)) {											// 12 hour notation?
		if (hour!=12)
			hour %= 12;
		strcpy(ampm, hour != 12 && hour == fhour ? "am" : "pm");
	}
	char sep = dos.tables.country[13];
	if (sec>=100&&msec>=100)
		sprintf(retBuf, "%2u%c%02u%c", (unsigned int)hour, sep, (unsigned int)min, *ampm);
	else
		sprintf(retBuf, "%u%c%02u%c%02u%c%02u%s", (unsigned int)hour, sep, (unsigned int)min, sep, (unsigned int)sec, dos.tables.country[9], (unsigned int)msec, ampm);
	return retBuf;
	}


struct DtaResult {
	char name[DOS_NAMELENGTH_ASCII];
	char lname[LFN_NAMELENGTH+1];
	Bit32u size;
	Bit16u date;
	Bit16u time;
	Bit8u attr;

	static bool groupDef(const DtaResult &lhs, const DtaResult &rhs) { return (lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY)?true:((((lhs.attr & DOS_ATTR_DIRECTORY) && (rhs.attr & DOS_ATTR_DIRECTORY)) || (!(lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY))) && strcmp(lhs.name, rhs.name) < 0); }
	static bool groupDirs(const DtaResult &lhs, const DtaResult &rhs) { return (lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY); }
	static bool compareName(const DtaResult &lhs, const DtaResult &rhs) { return strcmp(lhs.name, rhs.name) < 0; }
	static bool compareExt(const DtaResult &lhs, const DtaResult &rhs) { return strcmp(lhs.getExtension(), rhs.getExtension()) < 0; }
	static bool compareSize(const DtaResult &lhs, const DtaResult &rhs) { return lhs.size < rhs.size; }
	static bool compareDate(const DtaResult &lhs, const DtaResult &rhs) { return lhs.date < rhs.date || (lhs.date == rhs.date && lhs.time < rhs.time); }

	const char * getExtension() const {
		const char * ext = empty_string;
		if (name[0] != '.') {
			ext = strrchr(name, '.');
			if (!ext) ext = empty_string;
		}
		return ext;
	}

};

Bit32u byte_count,file_count,dir_count;
Bitu p_count;
std::vector<std::string> dirs, adirs;

static bool dirPaused(DOS_Shell * shell, Bitu w_size, bool optP, bool optW) {
	p_count+=optW?5:1;
	if (optP && p_count%(GetPauseCount()*w_size)<1) {
		shell->WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
		Bit8u c;Bit16u n=1;
		DOS_ReadFile(STDIN,&c,&n);
		if (c==3) {shell->WriteOut("^C\r\n");return false;}
		if (c==0) DOS_ReadFile(STDIN,&c,&n); // read extended key
	}
	return true;
}

static bool doDir(DOS_Shell * shell, char * args, DOS_DTA dta, char * numformat, Bitu w_size, bool optW, bool optZ, bool optS, bool optP, bool optB, bool optA, bool optAD, bool optAminusD, bool optAS, bool optAminusS, bool optAH, bool optAminusH, bool optAR, bool optAminusR, bool optAA, bool optAminusA, bool optO, bool optOG, bool optON, bool optOD, bool optOE, bool optOS, bool reverseSort) {
	char path[DOS_PATHLENGTH];
	char sargs[CROSS_LEN], largs[CROSS_LEN];

	/* Make a full path in the args */
	if (!DOS_Canonicalize(args,path)) {
		shell->WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return true;
	}
	*(strrchr(path,'\\')+1)=0;
	if (!DOS_GetSFNPath(path,sargs,false)) {
		shell->WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return true;
	}
    if (!optB&&!optS) {
		shell->WriteOut(MSG_Get("SHELL_CMD_DIR_INTRO"),uselfn&&!optZ&&DOS_GetSFNPath(path,largs,true)?largs:sargs);
		if (optP) {
			p_count+=optW?10:2;
			if (p_count%(GetPauseCount()*w_size)<2) {
				shell->WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
				Bit8u c;Bit16u n=1;
				DOS_ReadFile(STDIN,&c,&n);
				if (c==3) {shell->WriteOut("^C\r\n");return false;}
				if (c==0) DOS_ReadFile(STDIN,&c,&n); // read extended key
			}
		}
	}
    if (*(sargs+strlen(sargs)-1) != '\\') strcat(sargs,"\\");

	Bit32u cbyte_count=0,cfile_count=0,w_count=0;
	int fbak=lfn_filefind_handle;
	lfn_filefind_handle=uselfn&&!optZ?LFN_FILEFIND_INTERNAL:LFN_FILEFIND_NONE;
	bool ret=DOS_FindFirst(args,0xffff & ~DOS_ATTR_VOLUME), found=true, first=true;
	lfn_filefind_handle=fbak;
	if (ret) {
		std::vector<DtaResult> results;

		lfn_filefind_handle=uselfn&&!optZ?LFN_FILEFIND_INTERNAL:LFN_FILEFIND_NONE;
		do {    /* File name and extension */
			DtaResult result;
			dta.GetResult(result.name,result.lname,result.size,result.date,result.time,result.attr);

			/* Skip non-directories if option AD is present, or skip dirs in case of A-D */
			if(optAD && !(result.attr&DOS_ATTR_DIRECTORY) ) continue;
			else if(optAminusD && (result.attr&DOS_ATTR_DIRECTORY) ) continue;
			else if(optAS && !(result.attr&DOS_ATTR_SYSTEM) ) continue;
			else if(optAminusS && (result.attr&DOS_ATTR_SYSTEM) ) continue;
			else if(optAH && !(result.attr&DOS_ATTR_HIDDEN) ) continue;
			else if(optAminusH && (result.attr&DOS_ATTR_HIDDEN) ) continue;
			else if(optAR && !(result.attr&DOS_ATTR_READ_ONLY) ) continue;
			else if(optAminusR && (result.attr&DOS_ATTR_READ_ONLY) ) continue;
			else if(optAA && !(result.attr&DOS_ATTR_ARCHIVE) ) continue;
			else if(optAminusA && (result.attr&DOS_ATTR_ARCHIVE) ) continue;
			else if(!(optA||optAD||optAminusD||optAS||optAminusS||optAH||optAminusH||optAR||optAminusR||optAA||optAminusA) && (result.attr&DOS_ATTR_SYSTEM || result.attr&DOS_ATTR_HIDDEN) && strcmp(result.name, "..") ) continue;

			results.push_back(result);

		} while ( (ret=DOS_FindNext()) );
		lfn_filefind_handle=fbak;

		if (optON) {
			// Sort by name
			std::sort(results.begin(), results.end(), DtaResult::compareName);
		} else if (optOE) {
			// Sort by extension
			std::sort(results.begin(), results.end(), DtaResult::compareExt);
		} else if (optOD) {
			// Sort by date
			std::sort(results.begin(), results.end(), DtaResult::compareDate);
		} else if (optOS) {
			// Sort by size
			std::sort(results.begin(), results.end(), DtaResult::compareSize);
		} else if (optOG) {
			// Directories first, then files
			std::sort(results.begin(), results.end(), DtaResult::groupDirs);
		} else if (optO) {
			// Directories first, then files, both sort by name
			std::sort(results.begin(), results.end(), DtaResult::groupDef);
		}
		if (reverseSort) {
			std::reverse(results.begin(), results.end());
		}

		for (std::vector<DtaResult>::iterator iter = results.begin(); iter != results.end(); ++iter) {

			char * name = iter->name;
			char *lname = iter->lname;
			Bit32u size = iter->size;
			Bit16u date = iter->date;
			Bit16u time = iter->time;
			Bit8u attr = iter->attr;

			/* output the file */
			if (optB) {
				// this overrides pretty much everything
				if (strcmp(".",uselfn&&!optZ?lname:name) && strcmp("..",uselfn&&!optZ?lname:name)) {
					shell->WriteOut("%s\n",uselfn&&!optZ?lname:name);
				}
			} else {
				if (first&&optS) {
					first=false;
					shell->WriteOut("\n");
					shell->WriteOut(MSG_Get("SHELL_CMD_DIR_INTRO"),uselfn&&!optZ&&DOS_GetSFNPath(path,largs,true)?largs:sargs);
					if (optP) {
						p_count+=optW?15:3;
						if (optS&&p_count%(GetPauseCount()*w_size)<3) {
							shell->WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
							Bit8u c;Bit16u n=1;
							DOS_ReadFile(STDIN,&c,&n);
							if (c==3) {shell->WriteOut("^C\r\n");return false;}
							if (c==0) DOS_ReadFile(STDIN,&c,&n); // read extended key
						}
					}
				}
				char * ext = empty_string;
				if (!optW && (name[0] != '.')) {
					ext = strrchr(name, '.');
					if (!ext) ext = empty_string;
					else *ext++ = 0;
				}
				Bit8u day	= (Bit8u)(date & 0x001f);
				Bit8u month	= (Bit8u)((date >> 5) & 0x000f);
				Bit16u year = (Bit16u)((date >> 9) + 1980);
				Bit8u hour	= (Bit8u)((time >> 5 ) >> 6);
				Bit8u minute = (Bit8u)((time >> 5) & 0x003f);

				if (attr & DOS_ATTR_DIRECTORY) {
					if (optW) {
						shell->WriteOut("[%s]",name);
						size_t namelen = strlen(name);
						if (namelen <= 14) {
							for (size_t i=14-namelen;i>0;i--) shell->WriteOut(" ");
						}
					} else {
						shell->WriteOut("%-8s %-3s   %-16s %s %s %s\n",name,ext,"<DIR>",FormatDate(year,month,day),FormatTime(hour,minute,100,100),uselfn&&!optZ?lname:"");
					}
					dir_count++;
				} else {
					if (optW) {
						shell->WriteOut("%-16s",name);
					} else {
						FormatNumber(size,numformat);
						shell->WriteOut("%-8s %-3s   %16s %s %s %s\n",name,ext,numformat,FormatDate(year,month,day),FormatTime(hour,minute,100,100),uselfn&&!optZ?lname:"");
					}
					if (optS) {
						cfile_count++;
						cbyte_count+=size;
					}
					file_count++;
					byte_count+=size;
				}
				if (optW) w_count++;
			}
			if (optP && !(++p_count%(GetPauseCount()*w_size))) {
				if (optW&&w_count%5) {shell->WriteOut("\n");w_count=0;}
				shell->WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
				Bit8u c;Bit16u n=1;
				DOS_ReadFile(STDIN,&c,&n);
				if (c==3) {shell->WriteOut("^C\r\n");return false;}
				if (c==0) DOS_ReadFile(STDIN,&c,&n); // read extended key
			}
		}

		if (!results.size())
			found=false;
		else if (optW&&w_count%5)
			shell->WriteOut("\n");
	} else
		found=false;
	if (!found&&!optB&&!optS) {
		shell->WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
		if (!dirPaused(shell, w_size, optP, optW)) return false;
	}
	if (optS) {
		size_t len=strlen(sargs);
		strcat(sargs, "*.*");
		bool ret=DOS_FindFirst(sargs,0xffff & ~DOS_ATTR_VOLUME);
		*(sargs+len)=0;
		if (ret) {
			std::vector<std::string> cdirs;
			cdirs.clear();
			do {    /* File name and extension */
				DtaResult result;
				dta.GetResult(result.name,result.lname,result.size,result.date,result.time,result.attr);

				if(result.attr&DOS_ATTR_DIRECTORY && strcmp(result.name, ".")&&strcmp(result.name, "..")) {
					strcat(sargs, result.name);
					strcat(sargs, "\\");
					char *fname = strrchr(args, '\\');
					if (fname==NULL) fname=args;
					else fname++;
					strcat(sargs, fname);
					cdirs.push_back((sargs[0]!='"'&&sargs[strlen(sargs)-1]=='"'?"\"":"")+std::string(sargs));
					*(sargs+len)=0;
				}
			} while ( (ret=DOS_FindNext()) );
			dirs.insert(dirs.begin()+1, cdirs.begin(), cdirs.end());
		}
		if (found&&!optB) {
			FormatNumber(cbyte_count,numformat);
			shell->WriteOut(MSG_Get("SHELL_CMD_DIR_BYTES_USED"),cfile_count,numformat);
			if (!dirPaused(shell, w_size, optP, optW)) return false;
		}
	}
	return true;
}

void DOS_Shell::CMD_DIR(char * args) {
	HELP("DIR");
	char numformat[16];
	char path[DOS_PATHLENGTH];
	char sargs[CROSS_LEN];

	std::string line;
	if(GetEnvStr("DIRCMD",line)){
		std::string::size_type idx = line.find('=');
		std::string value=line.substr(idx +1 , std::string::npos);
		line = std::string(args) + " " + value;
		args=const_cast<char*>(line.c_str());
	}

	ScanCMDBool(args,"4"); /* /4 ignored (default) */
	bool optW=ScanCMDBool(args,"W");
	bool optP=ScanCMDBool(args,"P");
	if (ScanCMDBool(args,"WP") || ScanCMDBool(args,"PW")) optW=optP=true;
	if (ScanCMDBool(args,"-W")) optW=false;
	if (ScanCMDBool(args,"-P")) optP=false;
	bool optZ=ScanCMDBool(args,"Z");
	if (ScanCMDBool(args,"-Z")) optZ=false;
	bool optS=ScanCMDBool(args,"S");
	if (ScanCMDBool(args,"-S")) optS=false;
	bool optB=ScanCMDBool(args,"B");
	if (ScanCMDBool(args,"-B")) optB=false;
	bool optA=ScanCMDBool(args,"A");
	bool optAD=ScanCMDBool(args,"AD")||ScanCMDBool(args,"A:D");
	bool optAminusD=ScanCMDBool(args,"A-D");
	bool optAS=ScanCMDBool(args,"AS")||ScanCMDBool(args,"A:S");
	bool optAminusS=ScanCMDBool(args,"A-S");
	bool optAH=ScanCMDBool(args,"AH")||ScanCMDBool(args,"A:H");
	bool optAminusH=ScanCMDBool(args,"A-H");
	bool optAR=ScanCMDBool(args,"AR")||ScanCMDBool(args,"A:R");
	bool optAminusR=ScanCMDBool(args,"A-R");
	bool optAA=ScanCMDBool(args,"AA")||ScanCMDBool(args,"A:A");
	bool optAminusA=ScanCMDBool(args,"A-A");
	if (ScanCMDBool(args,"-A")) {
		optA = false;
		optAD = false;
		optAminusD = false;
		optAS = false;
		optAminusS = false;
		optAH = false;
		optAminusH = false;
		optAR = false;
		optAminusR = false;
		optAA = false;
		optAminusA = false;
	}
	// Sorting flags
	bool reverseSort = false;
	bool optON=ScanCMDBool(args,"ON")||ScanCMDBool(args,"O:N");
	if (ScanCMDBool(args,"O-N")) {
		optON = true;
		reverseSort = true;
	}
	bool optOD=ScanCMDBool(args,"OD")||ScanCMDBool(args,"O:D");
	if (ScanCMDBool(args,"O-D")) {
		optOD = true;
		reverseSort = true;
	}
	bool optOE=ScanCMDBool(args,"OE")||ScanCMDBool(args,"O:E");
	if (ScanCMDBool(args,"O-E")) {
		optOE = true;
		reverseSort = true;
	}
	bool optOS=ScanCMDBool(args,"OS")||ScanCMDBool(args,"O:S");
	if (ScanCMDBool(args,"O-S")) {
		optOS = true;
		reverseSort = true;
	}
	bool optOG=ScanCMDBool(args,"OG")||ScanCMDBool(args,"O:G");
	if (ScanCMDBool(args,"O-G")) {
		optOG = true;
		reverseSort = true;
	}
	bool optO=ScanCMDBool(args,"O");
	if (ScanCMDBool(args,"OGN")) optO=true;
	if (ScanCMDBool(args,"-O")) {
		optO = false;
		optOG = false;
		optON = false;
		optOD = false;
		optOE = false;
		optOS = false;
		reverseSort = false;
	}
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	byte_count=0;file_count=0;dir_count=0;p_count=0;
	Bitu w_size = optW?5:1;

	char buffer[CROSS_LEN];
	args = trim(args);
	size_t argLen = strlen(args);
	if (argLen == 0) {
		strcpy(args,"*.*"); //no arguments.
	} else {
		switch (args[argLen-1])
		{
		case '\\':	// handle \, C:\, etc.
		case ':' :	// handle C:, etc.
			strcat(args,"*.*");
			break;
		default:
			break;
		}
	}
	args = ExpandDot(args,buffer,CROSS_LEN);

	if (DOS_FindDevice(args) != DOS_DEVICES) {
		WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
		return;
	}
	if (!strrchr(args,'*') && !strrchr(args,'?')) {
		Bit16u attribute=0;
		if(!DOS_GetSFNPath(args,sargs,false)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			return;
		}
		if(DOS_GetFileAttr(sargs,&attribute) && (attribute&DOS_ATTR_DIRECTORY) ) {
			DOS_FindFirst(sargs,0xffff & ~DOS_ATTR_VOLUME);
			DOS_DTA dta(dos.dta());
			strcpy(args,sargs);
			strcat(args,"\\*.*");	// if no wildcard and a directory, get its files
		}
	}
	if (!DOS_GetSFNPath(args,sargs,false)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}
	if (!(uselfn&&!optZ&&strchr(sargs,'*'))&&!strrchr(sargs,'.'))
		strcat(sargs,".*");	// if no extension, get them all
    sprintf(args,"\"%s\"",sargs);

	/* Make a full path in the args */
	if (!DOS_Canonicalize(args,path)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}
	*(strrchr(path,'\\')+1)=0;
	if (!DOS_GetSFNPath(path,sargs,true)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}
    if (*(sargs+strlen(sargs)-1) != '\\') strcat(sargs,"\\");
    if (!optB) {
		if (strlen(sargs)>2&&sargs[1]==':') {
			char c[]=" _:";
			c[1]=toupper(sargs[0]);
			CMD_VOL(c[1]>='A'&&c[1]<='Z'?c:empty_string);
		} else
			CMD_VOL(empty_string);
		if (optP) p_count+=optW?15:3;
	}

	/* Command uses dta so set it to our internal dta */
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	dirs.clear();
	dirs.push_back(std::string(args));
	while (!dirs.empty()) {
		if (!doDir(this, (char *)dirs.begin()->c_str(), dta, numformat, w_size, optW, optZ, optS, optP, optB, optA, optAD, optAminusD, optAS, optAminusS, optAH, optAminusH, optAR, optAminusR, optAA, optAminusA, optO, optOG, optON, optOD, optOE, optOS, reverseSort)) {dos.dta(save_dta);return;}
		dirs.erase(dirs.begin());
	}
	if (!optB) {
		if (optS) {
			WriteOut("\n");
			if (!dirPaused(this, w_size, optP, optW)) {dos.dta(save_dta);return;}
			if (!file_count&&!dir_count)
				WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
			else
				WriteOut(MSG_Get("SHELL_CMD_DIR_FILES_LISTED"));
			if (!dirPaused(this, w_size, optP, optW)) {dos.dta(save_dta);return;}
		}
		/* Show the summary of results */
		FormatNumber(byte_count,numformat);
		WriteOut(MSG_Get("SHELL_CMD_DIR_BYTES_USED"),file_count,numformat);
		if (!dirPaused(this, w_size, optP, optW)) {dos.dta(save_dta);return;}
		Bit8u drive=dta.GetSearchDrive();
		//TODO Free Space
		Bitu free_space=1024u*1024u*100u;
		if (Drives[drive]) {
			Bit32u bytes_sector32;Bit32u sectors_cluster32;Bit32u total_clusters32;Bit32u free_clusters32;
			if ((dos.version.major > 7 || (dos.version.major == 7 && dos.version.minor >= 10)) &&
				Drives[drive]->AllocationInfo32(&bytes_sector32,&sectors_cluster32,&total_clusters32,&free_clusters32)) { /* FAT32 aware extended API */
				rsize=true;
				freec=0;
				free_space=(Bitu)bytes_sector32 * (Bitu)sectors_cluster32 * (Bitu)(freec?freec:free_clusters32);
				rsize=false;
			}
			else {
				Bit16u bytes_sector;Bit8u sectors_cluster;Bit16u total_clusters;Bit16u free_clusters;
				rsize=true;
				freec=0;
				Drives[drive]->AllocationInfo(&bytes_sector,&sectors_cluster,&total_clusters,&free_clusters);
				free_space=(Bitu)bytes_sector * (Bitu)sectors_cluster * (Bitu)(freec?freec:free_clusters);
				rsize=false;
			}
		}
		FormatNumber(free_space,numformat);
		WriteOut(MSG_Get("SHELL_CMD_DIR_BYTES_FREE"),dir_count,numformat);
		if (!dirPaused(this, w_size, optP, optW)) {dos.dta(save_dta);return;}
	}
	dos.dta(save_dta);
}

void DOS_Shell::CMD_LS(char *args) {
	//HELP("LS");
	bool optA=ScanCMDBool(args,"A");
	bool optL=ScanCMDBool(args,"L");
	bool optP=ScanCMDBool(args,"P");
	bool optZ=ScanCMDBool(args,"Z");
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}

	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());

	std::string pattern = args;
	trim(pattern);

	const char last_char = (pattern.length() > 0 ? pattern.back() : '\0');
	switch (last_char) {
		case '\0': // No arguments, search for all.
			pattern = "*.*";
			break;
		case '\\': // Handle \, C:\, etc.
		case ':':  // Handle C:, etc.
			pattern += "*.*";
			break;
		default: break;
	}

	// Handle patterns starting with a dot.
	char buffer[CROSS_LEN];
	pattern = ExpandDot((char *)pattern.c_str(), buffer, sizeof(buffer));

	// When there's no wildcard and target is a directory then search files
	// inside the directory.
	const char *p = pattern.c_str();
	if (!strrchr(p, '*') && !strrchr(p, '?')) {
		uint16_t attr = 0;
		if (DOS_GetFileAttr(p, &attr) && (attr & DOS_ATTR_DIRECTORY))
			pattern += "\\*.*";
	}

	// If no extension, list all files.
	// This makes patterns like foo* work.
	if (!strrchr(pattern.c_str(), '.'))
		pattern += ".*";

	char spattern[CROSS_LEN];
	if (!DOS_GetSFNPath(pattern.c_str(),spattern,false)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}
	int fbak=lfn_filefind_handle;
	lfn_filefind_handle=uselfn?LFN_FILEFIND_INTERNAL:LFN_FILEFIND_NONE;
	bool ret = DOS_FindFirst((char *)((uselfn?"\"":"")+std::string(spattern)+(uselfn?"\"":"")).c_str(), 0xffff & ~DOS_ATTR_VOLUME);
	if (!ret) {
		lfn_filefind_handle=fbak;
		if (strlen(trim(args)))
			WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"), trim(args));
		else
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		dos.dta(save_dta);
		return;
	}

	std::vector<DtaResult> results;
	// reserve space for as many as we can fit into a single memory page
	// nothing more to it; make it larger if necessary
	results.reserve(MEM_PAGE_SIZE / sizeof(DtaResult));

	do {
		DtaResult result;
		dta.GetResult(result.name, result.lname, result.size, result.date, result.time, result.attr);
		results.push_back(result);
	} while ((ret = DOS_FindNext()) == true);
	lfn_filefind_handle=fbak;

	size_t w_count, p_count, col;
	unsigned int max[15], total, tcols=IS_PC98_ARCH?80:real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
	if (!tcols) tcols=80;
	int mrow=tcols>80?15:10;

	for (col=mrow; col>0; col--) {
		for (int i=0; i<mrow; i++) max[i]=2;
		if (optL) col=1;
		if (col==1) break;
		w_count=0;
		for (const auto &entry : results) {
			std::string name = uselfn&&!optZ?entry.lname:entry.name;
			if (name == "." || name == "..") continue;
			if (!optA && (entry.attr&DOS_ATTR_SYSTEM || entry.attr&DOS_ATTR_HIDDEN)) continue;
			if (name.size()+2>max[w_count%col]) max[w_count%col]=(unsigned int)(name.size()+2);
			++w_count;
		}
		total=0;
		for (size_t i=0; i<col; i++) total+=max[i];
		if (total<tcols) break;
	}
	
	w_count = p_count = 0;

	for (const auto &entry : results) {
		std::string name = uselfn&&!optZ?entry.lname:entry.name;
		if (name == "." || name == "..") continue;		
		if (!optA && (entry.attr&DOS_ATTR_SYSTEM || entry.attr&DOS_ATTR_HIDDEN)) continue;
		if (entry.attr & DOS_ATTR_DIRECTORY) {
			if (!uselfn||optZ) upcase(name);
			if (col==1) {
				WriteOut("\033[34;1m%s\033[0m\n", name.c_str());
				p_count++;
			} else
				WriteOut("\033[34;1m%-*s\033[0m", max[w_count % col], name.c_str());
		} else {
			if (!uselfn||optZ) lowcase(name);
			bool is_executable=false;
			if (name.length()>4) {
				std::string ext=name.substr(name.length()-4);
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
				if (ext==".exe"||ext==".com"||ext==".bat") is_executable=true;
			}
			if (col==1) {
				WriteOut(is_executable?"\033[32;1m%s\033[0m\n":"%s\n", name.c_str());
				p_count++;
			} else
				WriteOut(is_executable?"\033[32;1m%-*s\033[0m":"%-*s", max[w_count % col], name.c_str());
		}
		if (col>1) {
			++w_count;
			if (w_count % col == 0) {p_count++;WriteOut_NoParsing("\n");}
		}
		if (optP&&p_count>=GetPauseCount()) {
			WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
			Bit8u c;Bit16u n=1;
			DOS_ReadFile(STDIN,&c,&n);
			if (c==3) {WriteOut("^C\r\n");dos.dta(save_dta);return;}
			if (c==0) DOS_ReadFile(STDIN,&c,&n); // read extended key
			p_count=0;
		}
	}
	if (col>1&&w_count%col) WriteOut_NoParsing("\n");
	dos.dta(save_dta);
}

struct copysource {
	std::string filename;
	bool concat;
	copysource(std::string& filein,bool concatin):
		filename(filein),concat(concatin){ };
	copysource():filename(""),concat(false){ };
};


void DOS_Shell::CMD_COPY(char * args) {
	HELP("COPY");
	static std::string defaulttarget = ".";
	StripSpaces(args);
	/* Command uses dta so set it to our internal dta */
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	Bit32u size;Bit16u date;Bit16u time;Bit8u attr;
	char name[DOS_NAMELENGTH_ASCII], lname[LFN_NAMELENGTH+1];
	std::vector<copysource> sources;
	// ignore /b and /t switches: always copy binary
	while(ScanCMDBool(args,"B")) ;
	while(ScanCMDBool(args,"T")) ; //Shouldn't this be A ?
	while(ScanCMDBool(args,"A")) ;
	bool optY=ScanCMDBool(args,"Y");
	std::string line;
	if(GetEnvStr("COPYCMD",line)){
		std::string::size_type idx = line.find('=');
		std::string value=line.substr(idx +1 , std::string::npos);
		char copycmd[CROSS_LEN];
		strcpy(copycmd, value.c_str());
		if (ScanCMDBool(copycmd, "Y") && !ScanCMDBool(copycmd, "-Y")) optY = true;
	}
	if (ScanCMDBool(args,"-Y")) optY=false;
	ScanCMDBool(args,"V");

	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		dos.dta(save_dta);
		return;
	}
	// Gather all sources (extension to copy more then 1 file specified at command line)
	// Concatenating files go as follows: All parts except for the last bear the concat flag.
	// This construction allows them to be counted (only the non concat set)
	char q[]="\"";
	char* source_p = NULL;
	char source_x[DOS_PATHLENGTH+CROSS_LEN];
	while ( (source_p = StripArg(args)) && *source_p ) {
		do {
			char* plus = strchr(source_p,'+');
			// If StripWord() previously cut at a space before a plus then
			// set concatenate flag on last source and remove leading plus.
			if (plus == source_p && sources.size()) {
				sources[sources.size()-1].concat = true;
				// If spaces also followed plus then item is only a plus.
				if (strlen(++source_p)==0) break;
				plus = strchr(source_p,'+');
			}
			if (plus) {
				char *c=source_p+strlen(source_p)-1;
				if (*source_p=='"'&&*c=='"') {
					*c=0;
					if (strchr(source_p+1,'"'))
						*plus++ = 0;
					else
						plus=NULL;
					*c='"';
				} else
					*plus++ = 0;
			}
			safe_strncpy(source_x,source_p,CROSS_LEN);
			bool has_drive_spec = false;
			size_t source_x_len = strlen(source_x);
			if (source_x_len>0) {
				if (source_x[source_x_len-1]==':') has_drive_spec = true;
				else if (uselfn&&strchr(source_x, '*')) {
					char * find_last;
					find_last=strrchr(source_x,'\\');
					if (find_last==NULL) find_last=source_x;
					else find_last++;
					if (strlen(find_last)>0&&source_x[source_x_len-1]=='*'&&strchr(find_last, '.')==NULL) strcat(source_x, ".*");
				}
			}
			if (!has_drive_spec  && !strpbrk(source_p,"*?") ) { //doubt that fu*\*.* is valid
                char spath[DOS_PATHLENGTH];
                if (DOS_GetSFNPath(source_p,spath,false)) {
					bool root=false;
					if (strlen(spath)==3&&spath[1]==':'&&spath[2]=='\\') {
						root=true;
						strcat(spath, "*.*");
					}
					if (DOS_FindFirst(spath,0xffff & ~DOS_ATTR_VOLUME)) {
                    dta.GetResult(name,lname,size,date,time,attr);
					if (attr & DOS_ATTR_DIRECTORY || root)
						strcat(source_x,"\\*.*");
					}
				}
			}
            std::string source_xString = std::string(source_x);
			sources.push_back(copysource(source_xString,(plus)?true:false));
			source_p = plus;
		} while(source_p && *source_p);
	}
	// At least one source has to be there
	if (!sources.size() || !sources[0].filename.size()) {
		WriteOut(MSG_Get("SHELL_MISSING_PARAMETER"));
		dos.dta(save_dta);
		return;
	}

	copysource target;
	// If more then one object exists and last target is not part of a 
	// concat sequence then make it the target.
	if(sources.size()>1 && !sources[sources.size()-2].concat){
		target = sources.back();
		sources.pop_back();
	}
	//If no target => default target with concat flag true to detect a+b+c
	if(target.filename.size() == 0) target = copysource(defaulttarget,true);

	copysource oldsource;
	copysource source;
	Bit32u count = 0;
	while(sources.size()) {
		/* Get next source item and keep track of old source for concat start end */
		oldsource = source;
		source = sources[0];
		sources.erase(sources.begin());

		//Skip first file if doing a+b+c. Set target to first file
		if(!oldsource.concat && source.concat && target.concat) {
			target = source;
			continue;
		}

		/* Make a full path in the args */
		char pathSourcePre[DOS_PATHLENGTH], pathSource[DOS_PATHLENGTH+2];
		char pathTarget[DOS_PATHLENGTH];

		if (!DOS_Canonicalize(const_cast<char*>(source.filename.c_str()),pathSourcePre)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			dos.dta(save_dta);
			return;
		}
		strcpy(pathSource,pathSourcePre);
		if (uselfn) sprintf(pathSource,"\"%s\"",pathSourcePre);
		// cut search pattern
		char* pos = strrchr(pathSource,'\\');
		if (pos) *(pos+1) = 0;

		if (!DOS_Canonicalize(const_cast<char*>(target.filename.c_str()),pathTarget)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			dos.dta(save_dta);
			return;
		}
		char* temp = strstr(pathTarget,"*.*");
		if(temp && (temp == pathTarget || temp[-1] == '\\')) *temp = 0;//strip off *.* from target
	
		// add '\\' if target is a directory
		bool target_is_file = true;
		if (pathTarget[strlen(pathTarget)-1]!='\\') {
			if (DOS_FindFirst(pathTarget,0xffff & ~DOS_ATTR_VOLUME)) {
				dta.GetResult(name,lname,size,date,time,attr);
				if (attr & DOS_ATTR_DIRECTORY) {
					strcat(pathTarget,"\\");
					target_is_file = false;
				}
			}
		} else target_is_file = false;

		//Find first sourcefile
		char sPath[DOS_PATHLENGTH];
		bool ret=DOS_GetSFNPath(source.filename.c_str(),sPath,false) && DOS_FindFirst((char *)((strchr(sPath, ' ')&&sPath[0]!='"'&&sPath[0]!='"'?"\"":"")+std::string(sPath)+(strchr(sPath, ' ')&&sPath[strlen(sPath)-1]!='"'?"\"":"")).c_str(),0xffff & ~DOS_ATTR_VOLUME);
		if (!ret) {
			WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),const_cast<char*>(source.filename.c_str()));
			dos.dta(save_dta);
			return;
		}

		Bit16u sourceHandle,targetHandle = 0;
		char nameTarget[DOS_PATHLENGTH];
		char nameSource[DOS_PATHLENGTH], nametmp[DOS_PATHLENGTH+2];
		
		// Cache so we don't have to recalculate
		size_t pathTargetLen = strlen(pathTarget);
		
		// See if we have to substitute filename or extension
		char *ext = 0;
		size_t replacementOffset = 0;
		if (pathTarget[pathTargetLen-1]!='\\') { 
				// only if it's not a directory
			ext = strchr(pathTarget, '.');
			if (ext > pathTarget) { // no possible substitution
				if (ext[-1] == '*') {
					// we substitute extension, save it, hide the name
					ext[-1] = 0;
					assert(ext > pathTarget + 1); // pathTarget is fully qualified
					if (ext[-2] != '\\') {
						// there is something before the asterisk
						// save the offset in the source names

						replacementOffset = source.filename.find('*');
						size_t lastSlash = source.filename.rfind('\\');
						if (std::string::npos == lastSlash)
							lastSlash = 0;
						else
							lastSlash++;
						if (std::string::npos == replacementOffset
							  || replacementOffset < lastSlash) {
							// no asterisk found or in wrong place, error
							WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
							dos.dta(save_dta);
							return;
						}
						replacementOffset -= lastSlash;
//						WriteOut("[II] replacement offset is %d\n", replacementOffset);
					}
				}
				if (ext[1] == '*') {
					// we substitute name, clear the extension
					*ext = 0;
				} else if (ext[-1]) {
					// we don't substitute anything, clear up
					ext = 0;
				}
			}
		}

		bool echo=dos.echo, second_file_of_current_source = false;
		while (ret) {
			dta.GetResult(name,lname,size,date,time,attr);

			if ((attr & DOS_ATTR_DIRECTORY)==0) {
                Bit16u ftime,fdate;

				strcpy(nameSource,pathSource);
				strcat(nameSource,name);
				
				// Open Source
				if (DOS_OpenFile(nameSource,0,&sourceHandle)) {
                    // record the file date/time
                    bool ftdvalid = DOS_GetFileDate(sourceHandle, &ftime, &fdate);
                    if (!ftdvalid) LOG_MSG("WARNING: COPY cannot obtain file date/time");

					// Create Target or open it if in concat mode
					strcpy(nameTarget,q);
                    strcat(nameTarget,pathTarget);
					
					if (ext) { // substitute parts if necessary
						if (!ext[-1]) { // substitute extension
							strcat(nameTarget, (uselfn?lname:name) + replacementOffset);
							char *p=strchr(nameTarget, '.');
							strcpy(p==NULL?nameTarget+strlen(nameTarget):p, ext);
						}
						if (ext[1] == '*') { // substitute name (so just add the extension)
							strcat(nameTarget, strchr(uselfn?lname:name, '.'));
						}
					}
					
                    if (nameTarget[strlen(nameTarget)-1]=='\\') strcat(nameTarget,uselfn?lname:name);
                    strcat(nameTarget,q);

					//Special variable to ensure that copy * a_file, where a_file is not a directory concats.
					bool special = second_file_of_current_source && target_is_file && strchr(target.filename.c_str(), '*')==NULL;
					second_file_of_current_source = true; 
					if (special) oldsource.concat = true;
					if (*nameSource&&*nameTarget) {
						strcpy(nametmp, nameSource[0]!='\"'&&nameTarget[0]=='\"'?"\"":"");
						strcat(nametmp, nameSource);
						strcat(nametmp, nameSource[strlen(nameSource)-1]!='\"'&&nameTarget[strlen(nameTarget)-1]=='\"'?"\"":"");
					} else
						strcpy(nametmp, nameSource);
					if (!oldsource.concat && (!strcasecmp(nameSource, nameTarget) || !strcasecmp(nametmp, nameTarget)))
						{
						WriteOut("File cannot be copied onto itself\r\n");
						dos.dta(save_dta);
						DOS_CloseFile(sourceHandle);
						if (targetHandle)
							DOS_CloseFile(targetHandle);
						return;
						}
					Bit16u fattr;
					bool exist = DOS_GetFileAttr(nameTarget, &fattr);
					if (!(attr & DOS_ATTR_DIRECTORY) && DOS_FindDevice(nameTarget) == DOS_DEVICES) {
						if (exist && !optY && !oldsource.concat) {
							dos.echo=false;
							WriteOut(MSG_Get("SHELL_CMD_COPY_CONFIRM"), nameTarget);
							Bit8u c;
							Bit16u n=1;
							while (true)
								{
								DOS_ReadFile (STDIN,&c,&n);
								if (c==3) {WriteOut("^C\r\n");dos.dta(save_dta);DOS_CloseFile(sourceHandle);dos.echo=echo;return;}
								if (c=='y'||c=='Y') {WriteOut("Y\r\n", c);break;}
								if (c=='n'||c=='N') {WriteOut("N\r\n", c);break;}
								if (c=='a'||c=='A') {WriteOut("A\r\n", c);optY=true;break;}
								}
							if (c=='n'||c=='N') {DOS_CloseFile(sourceHandle);ret = DOS_FindNext();continue;}
						}
						if (!exist&&size) {
							int drive=strlen(nameTarget)>1&&(nameTarget[1]==':'||nameTarget[2]==':')?(toupper(nameTarget[nameTarget[0]=='"'?1:0])-'A'):-1;
							if (drive>=0&&Drives[drive]) {
								Bit16u bytes_sector;Bit8u sectors_cluster;Bit16u total_clusters;Bit16u free_clusters;
								rsize=true;
								freec=0;
								Drives[drive]->AllocationInfo(&bytes_sector,&sectors_cluster,&total_clusters,&free_clusters);
								rsize=false;
								if ((Bitu)bytes_sector * (Bitu)sectors_cluster * (Bitu)(freec?freec:free_clusters)<size) {
									WriteOut(MSG_Get("SHELL_CMD_COPY_NOSPACE"), uselfn?lname:name);
									DOS_CloseFile(sourceHandle);
									ret = DOS_FindNext();
									continue;
								}
							}
						}
					}
					//Don't create a new file when in concat mode
					if (oldsource.concat || DOS_CreateFile(nameTarget,0,&targetHandle)) {
						Bit32u dummy=0;

                        if (DOS_FindDevice(name) == DOS_DEVICES && !DOS_SetFileDate(targetHandle, ftime, fdate))
                            LOG_MSG("WARNING: COPY unable to apply date/time to dest");

						//In concat mode. Open the target and seek to the eof
						if (!oldsource.concat || (DOS_OpenFile(nameTarget,OPEN_READWRITE,&targetHandle) && 
					        	                  DOS_SeekFile(targetHandle,&dummy,DOS_SEEK_END))) {
							// Copy 
							static Bit8u buffer[0x8000]; // static, otherwise stack overflow possible.
							bool	failed = false;
							Bit16u	toread = 0x8000;
							bool iscon=DOS_FindDevice(name)==DOS_FindDevice("con");
							if (iscon) dos.echo=true;
							bool cont;
							do {
								if (!DOS_ReadFile(sourceHandle,buffer,&toread)) failed=true;
								if (iscon)
									{
									if (dos.errorcode==77)
										{
										WriteOut("^C\r\n");
										dos.dta(save_dta);
										DOS_CloseFile(sourceHandle);
										DOS_CloseFile(targetHandle);
										if (!exist) DOS_UnlinkFile(nameTarget);
										dos.echo=echo;
										return;
										}
									cont=true;
									for (int i=0;i<toread;i++)
										if (buffer[i]==26)
											{
											toread=i;
											cont=false;
											break;
											}
									if (!DOS_WriteFile(targetHandle,buffer,&toread)) failed=true;
									if (cont) toread=0x8000;
									}
								else
									{
									if (!DOS_WriteFile(targetHandle,buffer,&toread)) failed=true;
									cont=toread == 0x8000;
									}
							} while (cont);
							if (!DOS_CloseFile(sourceHandle)) failed=true;
							if (!DOS_CloseFile(targetHandle)) failed=true;
							if (failed)
                                WriteOut(MSG_Get("SHELL_CMD_COPY_ERROR"),uselfn?lname:name);
                            else if (strcmp(name,lname)&&uselfn)
                                WriteOut(" %s [%s]\n",lname,name);
                            else
                                WriteOut(" %s\n",uselfn?lname:name);
							if(!source.concat && !special) count++; //Only count concat files once
						} else {
							DOS_CloseFile(sourceHandle);
							WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),const_cast<char*>(target.filename.c_str()));
						}
					} else {
						DOS_CloseFile(sourceHandle);
						WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),const_cast<char*>(target.filename.c_str()));
					}
				} else WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),const_cast<char*>(source.filename.c_str()));
			}
			//On to the next file if the previous one wasn't a device
			if ((attr&DOS_ATTR_DEVICE) == 0) ret = DOS_FindNext();
			else ret = false;
		}
	}

	WriteOut(MSG_Get("SHELL_CMD_COPY_SUCCESS"),count);
	dos.dta(save_dta);
	dos.echo=echo;
	Drives[DOS_GetDefaultDrive()]->EmptyCache();
}

/* NTS: WARNING, this function modifies the buffer pointed to by char *args */
void DOS_Shell::CMD_SET(char * args) {
	std::string line;

	HELP("SET");
	StripSpaces(args);

	if (*args == 0) { /* "SET" by itself means to show the environment block */
		Bitu count = GetEnvCount();

		for (Bitu a = 0;a < count;a++) {
			if (GetEnvNum(a,line))
				WriteOut("%s\n",line.c_str());			
		}

	}
	else {
		char *p;

		{ /* parse arguments at the start */
			char *pcheck = args;

			while (*pcheck != 0 && (*pcheck == ' ' || *pcheck == '\t')) pcheck++;
			if (*pcheck != 0 && strlen(pcheck) > 3 && (strncasecmp(pcheck,"/p ",3) == 0))
				E_Exit("Set /P is not supported. Use Choice!"); /* TODO: What is SET /P supposed to do? */
		}

		/* Most SET commands take the form NAME=VALUE */
		p = strchr(args,'=');
		if (p == NULL) {
			/* SET <variable> without assignment prints the variable instead */
			if (!GetEnvStr(args,line)) WriteOut(MSG_Get("SHELL_CMD_SET_NOT_SET"),args);
			WriteOut("%s\n",line.c_str());
		} else {
			/* ASCIIZ snip the args string in two, so that args is C-string name of the variable,
			 * and "p" is C-string value of the variable */
			*p++ = 0;

			/* No parsing is needed. The command interpreter does the variable substitution for us */
			if (!SetEnv(args,p)) {
				/* NTS: If Win95 is any example, the command interpreter expands the variables for us */
				WriteOut(MSG_Get("SHELL_CMD_SET_OUT_OF_SPACE"));
			}
		}
	}
}

void DOS_Shell::CMD_IF(char * args) {
	HELP("IF");
	StripSpaces(args,'=');
	bool has_not=false;

	while (strncasecmp(args,"NOT",3) == 0) {
		if (!isspace(*reinterpret_cast<unsigned char*>(&args[3])) && (args[3] != '=')) break;
		args += 3;	//skip text
		//skip more spaces
		StripSpaces(args,'=');
		has_not = !has_not;
	}

	if(strncasecmp(args,"ERRORLEVEL",10) == 0) {
		args += 10;	//skip text
		//Strip spaces and ==
		StripSpaces(args,'=');
		char* word = StripWord(args);
		if(!isdigit(*word)) {
			WriteOut(MSG_Get("SHELL_CMD_IF_ERRORLEVEL_MISSING_NUMBER"));
			return;
		}

		Bit8u n = 0;
		do n = n * 10 + (*word - '0');
		while (isdigit(*++word));
		if(*word && !isspace(*word)) {
			WriteOut(MSG_Get("SHELL_CMD_IF_ERRORLEVEL_INVALID_NUMBER"));
			return;
		}
		/* Read the error code from DOS */
		if ((dos.return_code>=n) ==(!has_not)) DoCommand(args);
		return;
	}

	if(strncasecmp(args,"EXIST ",6) == 0) {
		args += 6; //Skip text
		StripSpaces(args);
		char* word = StripArg(args);
		if (!*word) {
			WriteOut(MSG_Get("SHELL_CMD_IF_EXIST_MISSING_FILENAME"));
			return;
		}

		{	/* DOS_FindFirst uses dta so set it to our internal dta */
			char spath[DOS_PATHLENGTH], path[DOS_PATHLENGTH], pattern[DOS_PATHLENGTH], full[DOS_PATHLENGTH], *r;
			if (!DOS_Canonicalize(word,full)) return;
			r=strrchr(full, '\\');
			if (r!=NULL) {
				*r=0;
				strcpy(path, full);
				strcat(path, "\\");
				strcpy(pattern, r+1);
				*r='\\';
			} else {
				strcpy(path, "");
				strcpy(pattern, full);
			}
			int k=0;
			for (int i=0;i<(int)strlen(pattern);i++)
				if (pattern[i]!='\"')
					pattern[k++]=pattern[i];
			pattern[k]=0;
			strcpy(spath, path);
			if (strchr(word,'\"')||uselfn) {
				if (!DOS_GetSFNPath(("\""+std::string(path)+"\\").c_str(), spath, false)) strcpy(spath, path);
				if (!strlen(spath)||spath[strlen(spath)-1]!='\\') strcat(spath, "\\");
			}
			RealPt save_dta=dos.dta();
			dos.dta(dos.tables.tempdta);
			int fbak=lfn_filefind_handle;
			lfn_filefind_handle=uselfn?LFN_FILEFIND_INTERNAL:LFN_FILEFIND_NONE;
			std::string sfull=std::string(spath)+std::string(pattern);
			bool ret=DOS_FindFirst((char *)((uselfn&&sfull.length()&&sfull[0]!='"'?"\"":"")+sfull+(uselfn&&sfull.length()&&sfull[sfull.length()-1]!='"'?"\"":"")).c_str(),0xffff & ~(DOS_ATTR_VOLUME|DOS_ATTR_DIRECTORY));
			lfn_filefind_handle=fbak;
			dos.dta(save_dta);
			if (ret==(!has_not)) DoCommand(args);
		}
		return;
	}

	/* Normal if string compare */

	char* word1 = args;
	// first word is until space or =
	while (*args && !isspace(*reinterpret_cast<unsigned char*>(args)) && (*args != '='))
		args++;
	char* end_word1 = args;

	// scan for =
	while (*args && (*args != '='))
		args++;
	// check for ==
	if ((*args==0) || (args[1] != '=')) {
		SyntaxError();
		return;
	}
	args += 2;
	StripSpaces(args,'=');

	char* word2 = args;
	// second word is until space or =
	while (*args && !isspace(*reinterpret_cast<unsigned char*>(args)) && (*args != '='))
		args++;

	if (*args) {
		*end_word1 = 0;		// mark end of first word
		*args++ = 0;		// mark end of second word
		StripSpaces(args,'=');

		if ((strcmp(word1,word2)==0)==(!has_not)) DoCommand(args);
	}
}

void DOS_Shell::CMD_GOTO(char * args) {
	HELP("GOTO");
	StripSpaces(args);
	if (!bf) return;
	if (*args==':') args++;
	//label ends at the first space
	char* non_space = args;
	while (*non_space) {
		if((*non_space == ' ') || (*non_space == '\t')) 
			*non_space = 0; 
		else non_space++;
	}
	if (!*args) {
		WriteOut(MSG_Get("SHELL_CMD_GOTO_MISSING_LABEL"));
		return;
	}
	if (!bf->Goto(args)) {
		WriteOut(MSG_Get("SHELL_CMD_GOTO_LABEL_NOT_FOUND"),args);
		return;
	}
}

void DOS_Shell::CMD_SHIFT(char * args ) {
	HELP("SHIFT");
	if(bf) bf->Shift();
}

void DOS_Shell::CMD_TYPE(char * args) {
	HELP("TYPE");
		
	// ignore /p /h and /t for compatibility
	ScanCMDBool(args,"P");
	ScanCMDBool(args,"H");
	ScanCMDBool(args,"T");
	StripSpaces(args);
	if (strcasecmp(args,"nul")==0) return;
	if (!*args) {
		WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
		return;
	}
	Bit16u handle;
	char * word;
nextfile:
	word=StripArg(args);
	if (!DOS_OpenFile(word,0,&handle)) {
		WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),word);
		return;
	}
	Bit8u c;Bit16u n=1;
	bool iscon=DOS_FindDevice(word)==DOS_FindDevice("con");
	while (n) {
		DOS_ReadFile(handle,&c,&n);
		if (n==0 || c==0x1a) break; // stop at EOF
		if (iscon) {
			if (c==3) {WriteOut("^C\r\n");break;}
			else if (c==13) WriteOut("\r\n");
		}
		DOS_WriteFile(STDOUT,&c,&n);
	}
	DOS_CloseFile(handle);
	if (*args) goto nextfile;
}

void DOS_Shell::CMD_REM(char * args) {
	HELP("REM");
}

static char PAUSED(void) {
	Bit8u c; Bit16u n=1, handle;
	if (!usecon&&DOS_OpenFile("con", OPEN_READWRITE, &handle)) {
		DOS_ReadFile (handle,&c,&n);
		DOS_CloseFile(handle);
	} else
		DOS_ReadFile (STDIN,&c,&n);
	return c;
}

void DOS_Shell::CMD_MORE(char * args) {
	HELP("MORE");
	//ScanCMDBool(args,">");
	int nchars = 0, nlines = 0, linecount = 0, LINES = 25, COLS = 80, TABSIZE = 8;
	char * word;
	Bit8u c, last=0;
	Bit16u n=1;
	StripSpaces(args);
	if (IS_PC98_ARCH) {
		LINES=real_readb(0x60,0x113) & 0x01 ? 25 : 20;
		COLS=80;
	} else {
		LINES=real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1;
		COLS=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
	}
	LINES--;
	if(!*args||!strcasecmp(args, "con")) {
		while (true) {
			DOS_ReadFile (STDIN,&c,&n);
			if (c==3) {WriteOut("^C\r\n");dos.echo=echo;return;}
			else if (n==0) {if (last!=10) WriteOut("\r\n");dos.echo=echo;return;}
			else if (c==13&&last==26) {dos.echo=echo;return;}
			else {
				if (c==10);
				else if (c==13) {
					linecount++;
					WriteOut("\r\n");
				} else if (c=='\t') {
					do {
						WriteOut(" ");
						nchars++;
					} while ( nchars < COLS && nchars % TABSIZE );
				} else {
					nchars++;
					WriteOut("%c", c);
				}
				if (c == 13 || nchars >= COLS) {
					nlines++;
					nchars = 0;
					if (nlines == LINES) {
						WriteOut("-- More -- (%u) --",linecount);
						if (PAUSED()==3) {WriteOut("^C\r\n");return;}
						WriteOut("\n");
						nlines=0;
					}
				}
				last=c;
			}
		}
	}
	if (strcasecmp(args,"nul")==0) return;
	if (!*args) {
		WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
		return;
	}
	Bit16u handle;
nextfile:
	word=StripArg(args);
	if (!DOS_OpenFile(word,0,&handle)) {
		WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),word);
		return;
	}
	do {
		n=1;
		DOS_ReadFile(handle,&c,&n);
		DOS_WriteFile(STDOUT,&c,&n);
		if (c != '\t') nchars++;
		else do {
			WriteOut(" ");
			nchars++;
		} while ( nchars < COLS && nchars % TABSIZE );

		if (c == '\n') linecount++;
		if ((c == '\n') || (nchars >= COLS)) {
			nlines++;
			nchars = 0;
			if (nlines == LINES) {
				WriteOut("-- More -- %s (%u) --",word,linecount);
				if (PAUSED()==3) {WriteOut("^C\r\n");return;}
				WriteOut("\n");
				nlines=0;
			}
		}
	} while (n);
	DOS_CloseFile(handle);
	if (*args) {
		WriteOut("\n");
		if (PAUSED()==3) {WriteOut("^C\r\n");return;}
		goto nextfile;
	}
}

void DOS_Shell::CMD_PAUSE(char * args){
	HELP("PAUSE");
	if(args && *args) {
		args++;
		WriteOut("%s\n",args);	// optional specified message
	} else
	WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
	Bit8u c;Bit16u n=1;
	DOS_ReadFile(STDIN,&c,&n);
	if (c==0) DOS_ReadFile(STDIN,&c,&n); // read extended key
}

void DOS_Shell::CMD_CALL(char * args){
	HELP("CALL");
	this->call=true; /* else the old batchfile will be closed first */
	this->ParseLine(args);
	this->call=false;
}

void DOS_Shell::CMD_DATE(char * args) {
	HELP("DATE");	
	if(ScanCMDBool(args,"H")) {
		// synchronize date with host parameter
		time_t curtime;
		struct tm *loctime;
		curtime = time (NULL);
		loctime = localtime (&curtime);
		
		reg_cx = loctime->tm_year+1900;
		reg_dh = loctime->tm_mon+1;
		reg_dl = loctime->tm_mday;

		reg_ah=0x2b; // set system date
		CALLBACK_RunRealInt(0x21);
		return;
	}
	// check if a date was passed in command line
	char c=dos.tables.country[11], c1, c2;
	Bit32u newday,newmonth,newyear;
	int n=dos.tables.country[0]==1?sscanf(args,"%u%c%u%c%u",&newday,&c1,&newmonth,&c2,&newyear):(dos.tables.country[0]==2?sscanf(args,"%u%c%u%c%u",&newyear,&c1,&newmonth,&c2,&newday):sscanf(args,"%u%c%u%c%u",&newmonth,&c1,&newday,&c2,&newyear));
	if (n==5 && c1==c && c2==c) {
		reg_cx = static_cast<Bit16u>(newyear);
		reg_dh = static_cast<Bit8u>(newmonth);
		reg_dl = static_cast<Bit8u>(newday);

		reg_ah=0x2b; // set system date
		CALLBACK_RunRealInt(0x21);
		if(reg_al==0xff) WriteOut(MSG_Get("SHELL_CMD_DATE_ERROR"));
		return;
	}
	// display the current date
	reg_ah=0x2a; // get system date
	CALLBACK_RunRealInt(0x21);

	const char* datestring = MSG_Get("SHELL_CMD_DATE_DAYS");
	Bit32u length;
	char day[6] = {0};
	if(sscanf(datestring,"%u",&length) && (length<5) && (strlen(datestring)==((size_t)length*7+1))) {
		// date string appears valid
		for(Bit32u i = 0; i < length; i++) day[i] = datestring[reg_al*length+1+i];
	}
	bool dateonly = ScanCMDBool(args,"T");
	if(!dateonly) WriteOut(MSG_Get("SHELL_CMD_DATE_NOW"));
	
	if(date_host_forced) {
		time_t curtime;

		struct tm *loctime;
		curtime = time (NULL);

		loctime = localtime (&curtime);
		int hosty=loctime->tm_year+1900;
		int hostm=loctime->tm_mon+1;
		int hostd=loctime->tm_mday;
		if (hostm == 1 || hostm == 2) hosty--;
		hostm = (hostm + 9) % 12 + 1;
		int y = hosty % 100;
		int century = hosty / 100;
		int week = ((13 * hostm - 1) / 5 + hostd + y + y/4 + century/4 - 2*century) % 7;
		if (week < 0) week = (week + 7) % 7;

		const char* my_week[7]={"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
		WriteOut("%s %s\n",my_week[week],FormatDate((Bit16u)reg_cx, (Bit8u)reg_dh, (Bit8u)reg_dl));
	} else
		WriteOut("%s %s\n",day, FormatDate((Bit16u)reg_cx, (Bit8u)reg_dh, (Bit8u)reg_dl));
	if(!dateonly) {
		char format[11];
		sprintf(format, dos.tables.country[0]==1?"DD%cMM%cYYYY":(dos.tables.country[0]==2?"YYYY%cMM%cDD":"MM%cDD%cYYYY"), c, c);
		WriteOut(MSG_Get("SHELL_CMD_DATE_SETHLP"), format);
	}
}

void DOS_Shell::CMD_TIME(char * args) {
	HELP("TIME");
	if(ScanCMDBool(args,"H")) {
		// synchronize time with host parameter
		time_t curtime;
		struct tm *loctime;
		curtime = time (NULL);
		loctime = localtime (&curtime);
		
		//reg_cx = loctime->;
		//reg_dh = loctime->;
		//reg_dl = loctime->;

		// reg_ah=0x2d; // set system time TODO
		// CALLBACK_RunRealInt(0x21);
		
		Bit32u ticks=(Bit32u)(((double)(loctime->tm_hour*3600+
										loctime->tm_min*60+
										loctime->tm_sec))*18.206481481);
		mem_writed(BIOS_TIMER,ticks);
		return;
	}
	Bit32u newhour,newminute,newsecond;
	char c=dos.tables.country[13], c1, c2;
	if (sscanf(args,"%u%c%u%c%u",&newhour,&c1,&newminute,&c2,&newsecond)==5 && c1==c && c2==c) {
		//reg_ch = static_cast<Bit16u>(newhour);
		//reg_cl = static_cast<Bit8u>(newminute);
		//reg_dx = static_cast<Bit8u>(newsecond)<<8;

		//reg_ah=0x2d; // set system time
		//CALLBACK_RunRealInt(0x21);
		//if(reg_al==0xff) WriteOut(MSG_Get("SHELL_CMD_TIME_ERROR"));

		if( newhour > 23 || newminute > 59 || newsecond > 59)
			WriteOut(MSG_Get("SHELL_CMD_TIME_ERROR"));
		else {
			Bit32u ticks=(Bit32u)(((double)(newhour*3600+
											newminute*60+
											newsecond))*18.206481481);
			mem_writed(BIOS_TIMER,ticks);
		}
		return;
	}
	bool timeonly = ScanCMDBool(args,"T");

	reg_ah=0x2c; // get system time
	CALLBACK_RunRealInt(0x21);
/*
		reg_dl= // 1/100 seconds
		reg_dh= // seconds
		reg_cl= // minutes
		reg_ch= // hours
*/
	if(timeonly) {
		WriteOut("%2u:%02u\n",reg_ch,reg_cl);
	} else {
		WriteOut(MSG_Get("SHELL_CMD_TIME_NOW"));
		WriteOut("%s\n", FormatTime(reg_ch,reg_cl,reg_dh,reg_dl));
		char format[9];
		sprintf(format, "hh%cmm%css", dos.tables.country[13], dos.tables.country[13]);
		WriteOut(MSG_Get("SHELL_CMD_TIME_SETHLP"), format);
	}
}

void DOS_Shell::CMD_SUBST(char * args) {
/* If more that one type can be substed think of something else 
 * E.g. make basedir member dos_drive instead of localdrive
 */
	HELP("SUBST");
	try {
		char mountstring[DOS_PATHLENGTH+CROSS_LEN+20];
		strcpy(mountstring,"MOUNT ");
		StripSpaces(args);
		std::string arg;
		CommandLine command(0,args);
		if (!command.GetCount()) {
			char name[DOS_NAMELENGTH_ASCII],lname[LFN_NAMELENGTH];
			Bit32u size;Bit16u date;Bit16u time;Bit8u attr;
			/* Command uses dta so set it to our internal dta */
			RealPt save_dta = dos.dta();
			dos.dta(dos.tables.tempdta);
			DOS_DTA dta(dos.dta());

			WriteOut(MSG_Get("SHELL_CMD_SUBST_DRIVE_LIST"));
			WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_FORMAT"),"Drive","Type","Label");
			int cols=IS_PC98_ARCH?80:real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
			if (!cols) cols=80;
			for(int p = 0;p < cols;p++) WriteOut("-");
			bool none=true;
			for (int d = 0;d < DOS_DRIVES;d++) {
				if (!Drives[d]||strncmp(Drives[d]->GetInfo(),"local ",6)) continue;

				char root[7] = {(char)('A'+d),':','\\','*','.','*',0};
				bool ret = DOS_FindFirst(root,DOS_ATTR_VOLUME);
				if (ret) {
					dta.GetResult(name,lname,size,date,time,attr);
					DOS_FindNext(); //Mark entry as invalid
				} else name[0] = 0;

				/* Change 8.3 to 11.0 */
				const char* dot = strchr(name, '.');
				if(dot && (dot - name == 8) ) { 
					name[8] = name[9];name[9] = name[10];name[10] = name[11];name[11] = 0;
				}

				root[1] = 0; //This way, the format string can be reused.
				WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_FORMAT"),root, Drives[d]->GetInfo(),name);
                none=false;
			}
            if (none) WriteOut(MSG_Get("PROGRAM_IMGMOUNT_STATUS_NONE"));
			dos.dta(save_dta);
			return;
		}

		if (command.GetCount() != 2) throw 0 ;
  
		command.FindCommand(1,arg);
		if( (arg.size()>1) && arg[1] !=':')  throw(0);
		char temp_str[2] = { 0,0 };
		temp_str[0]=(char)toupper(args[0]);
		command.FindCommand(2,arg);
		if((arg=="/D") || (arg=="/d")) {
			if(!Drives[temp_str[0]-'A'] ) throw 1; //targetdrive not in use
			strcat(mountstring,"-u ");
			strcat(mountstring,temp_str);
			this->ParseLine(mountstring);
			return;
		}
		if(Drives[temp_str[0]-'A'] ) throw 2; //targetdrive in use
		strcat(mountstring,temp_str);
		strcat(mountstring," ");

        Bit8u drive;char dir[DOS_PATHLENGTH+2],fulldir[DOS_PATHLENGTH];
        if (strchr(arg.c_str(),'\"')==NULL)
            sprintf(dir,"\"%s\"",arg.c_str());
        else strcpy(dir,arg.c_str());
        if (!DOS_MakeName(dir,fulldir,&drive)) throw 3;
	
		localDrive* ldp=0;
		if( ( ldp=dynamic_cast<localDrive*>(Drives[drive])) == 0 ) throw 4;
		char newname[CROSS_LEN];   
		strcpy(newname, ldp->basedir);	   
		strcat(newname,fulldir);
		CROSS_FILENAME(newname);
		ldp->dirCache.ExpandName(newname);
		strcat(mountstring,"\"");	   
		strcat(mountstring, newname);
		strcat(mountstring,"\"");	   
		this->ParseLine(mountstring);
	}
	catch(int a){
		switch (a) {
			case 1:
		       	WriteOut(MSG_Get("SHELL_CMD_SUBST_NO_REMOVE"));
				break;
			case 2:
				WriteOut(MSG_Get("SHELL_CMD_SUBST_IN_USE"));
				break;
			case 3:
				WriteOut(MSG_Get("SHELL_CMD_SUBST_INVALID_PATH"));
				break;
			case 4:
				WriteOut(MSG_Get("SHELL_CMD_SUBST_NOT_LOCAL"));
				break;
			default:
				WriteOut(MSG_Get("SHELL_CMD_SUBST_FAILURE"));
		}
		return;
	}
	catch(...) {		//dynamic cast failed =>so no localdrive
		WriteOut(MSG_Get("SHELL_CMD_SUBST_FAILURE"));
		return;
	}
   
	return;
}

void DOS_Shell::CMD_LOADHIGH(char *args){
	HELP("LOADHIGH");
	Bit16u umb_start=dos_infoblock.GetStartOfUMBChain();
	Bit8u umb_flag=dos_infoblock.GetUMBChainState();
	Bit8u old_memstrat=(Bit8u)(DOS_GetMemAllocStrategy()&0xff);
	if (umb_start==0x9fff) {
		if ((umb_flag&1)==0) DOS_LinkUMBsToMemChain(1);
		DOS_SetMemAllocStrategy(0x80);	// search in UMBs first
		this->ParseLine(args);
		Bit8u current_umb_flag=dos_infoblock.GetUMBChainState();
		if ((current_umb_flag&1)!=(umb_flag&1)) DOS_LinkUMBsToMemChain(umb_flag);
		DOS_SetMemAllocStrategy(old_memstrat);	// restore strategy
	} else this->ParseLine(args);
}

void DOS_Shell::CMD_CHOICE(char * args){
	HELP("CHOICE");
	static char defchoice[3] = {'y','n',0};
	char *rem = NULL, *ptr;
	bool optN = ScanCMDBool(args,"N");
	bool optS = ScanCMDBool(args,"S"); //Case-sensitive matching
	// ignore /b and /m switches for compatibility
	ScanCMDBool(args,"B");
	ScanCMDBool(args,"M"); // Text
	ScanCMDBool(args,"T"); //Default Choice after timeout
	if (args) {
		char *last = strchr(args,0);
		StripSpaces(args);
		rem = ScanCMDRemain(args);
		if (rem && *rem && (tolower(rem[1]) != 'c')) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
			return;
		}
		if (args == rem) args = strchr(rem,0)+1;
		if (rem) rem += 2;
		if(rem && rem[0]==':') rem++; /* optional : after /c */
		if (args > last) args = NULL;
	}
	if (!rem || !*rem) rem = defchoice; /* No choices specified use YN */
	ptr = rem;
	Bit8u c;
	if(!optS) while ((c = (Bit8u)(*ptr))) *ptr++ = (char)toupper(c); /* When in no case-sensitive mode. make everything upcase */
	if(args && *args ) {
		StripSpaces(args);
		size_t argslen = strlen(args);
		if(argslen>1 && args[0] == '"' && args[argslen-1] =='"') {
			args[argslen-1] = 0; //Remove quotes
			args++;
		}
		WriteOut(args);
	}
	/* Show question prompt of the form [a,b]? where a b are the choice values */
	if (!optN) {
		if(args && *args) WriteOut(" ");
		WriteOut("[");
		size_t len = strlen(rem);
		for(size_t t = 1; t < len; t++) {
			WriteOut("%c,",rem[t-1]);
		}
		WriteOut("%c]?",rem[len-1]);
	}

	Bit16u n=1;
	do {
		DOS_ReadFile (STDIN,&c,&n);
		if (c==3) {WriteOut("^C\r\n");dos.return_code=0;return;}
	} while (!c || !(ptr = strchr(rem,(optS?c:toupper(c)))));
	c = optS?c:(Bit8u)toupper(c);
	DOS_WriteFile (STDOUT,&c, &n);
	c = '\n'; DOS_WriteFile (STDOUT,&c, &n);
	dos.return_code = (Bit8u)(ptr-rem+1);
}

static bool doAttrib(DOS_Shell * shell, char * args, DOS_DTA dta, bool optS, bool adda, bool adds, bool addh, bool addr, bool suba, bool subs, bool subh, bool subr) {
    char spath[DOS_PATHLENGTH],sargs[DOS_PATHLENGTH+4],path[DOS_PATHLENGTH+4],full[DOS_PATHLENGTH],sfull[DOS_PATHLENGTH+2];
	if (!DOS_Canonicalize(args,full)||strrchr(full,'\\')==NULL) { shell->WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));return false; }
	if (!DOS_GetSFNPath(args,spath,false)) {
		shell->WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
		return false;
	}
	if (!uselfn||!DOS_GetSFNPath(args,sfull,true)) strcpy(sfull,full);
    sprintf(sargs,"\"%s\"",spath);
    bool found=false, res=DOS_FindFirst(sargs,0xffff & ~DOS_ATTR_VOLUME);
	if (!res&&!optS) return false;
	//end can't be 0, but if it is we'll get a nice crash, who cares :)
	strcpy(path,full);
	*(strrchr(path,'\\')+1)=0;
	char * end=strrchr(full,'\\')+1;*end=0;
	char * lend=strrchr(sfull,'\\')+1;*lend=0;
    char name[DOS_NAMELENGTH_ASCII],lname[LFN_NAMELENGTH+1];
    Bit32u size;Bit16u time,date;Bit8u attr;Bit16u fattr;
	while (res) {
		dta.GetResult(name,lname,size,date,time,attr);
		if (!((!strcmp(name, ".") || !strcmp(name, "..") || strchr(sargs, '*')!=NULL || strchr(sargs, '?')!=NULL) && attr & DOS_ATTR_DIRECTORY)) {
			found=true;
			strcpy(end,name);
			strcpy(lend,lname);
			if (strlen(full)&&DOS_GetFileAttr(((uselfn||strchr(full, ' ')?(full[0]!='"'?"\"":""):"")+std::string(full)+(uselfn||strchr(full, ' ')?(full[strlen(full)-1]!='"'?"\"":""):"")).c_str(), &fattr)) {
				bool attra=fattr&DOS_ATTR_ARCHIVE, attrs=fattr&DOS_ATTR_SYSTEM, attrh=fattr&DOS_ATTR_HIDDEN, attrr=fattr&DOS_ATTR_READ_ONLY;
				if (adda||adds||addh||addr||suba||subs||subh||subr) {
					if (adda) fattr|=DOS_ATTR_ARCHIVE;
					if (adds) fattr|=DOS_ATTR_SYSTEM;
					if (addh) fattr|=DOS_ATTR_HIDDEN;
					if (addr) fattr|=DOS_ATTR_READ_ONLY;
					if (suba) fattr&=~DOS_ATTR_ARCHIVE;
					if (subs) fattr&=~DOS_ATTR_SYSTEM;
					if (subh) fattr&=~DOS_ATTR_HIDDEN;
					if (subr) fattr&=~DOS_ATTR_READ_ONLY;
					if (DOS_SetFileAttr(((uselfn||strchr(full, ' ')?(full[0]!='"'?"\"":""):"")+std::string(full)+(uselfn||strchr(full, ' ')?(full[strlen(full)-1]!='"'?"\"":""):"")).c_str(), fattr)) {
						if (DOS_GetFileAttr(((uselfn||strchr(full, ' ')?(full[0]!='"'?"\"":""):"")+std::string(full)+(uselfn||strchr(full, ' ')?(full[strlen(full)-1]!='"'?"\"":""):"")).c_str(), &fattr))
							shell->WriteOut("  %c  %c%c%c	%s\n", fattr&DOS_ATTR_ARCHIVE?'A':' ', fattr&DOS_ATTR_SYSTEM?'S':' ', fattr&DOS_ATTR_HIDDEN?'H':' ', fattr&DOS_ATTR_READ_ONLY?'R':' ', uselfn?sfull:full);
					} else
						shell->WriteOut(MSG_Get("SHELL_CMD_ATTRIB_SET_ERROR"),uselfn?sfull:full);
				} else
					shell->WriteOut("  %c  %c%c%c	%s\n", attra?'A':' ', attrs?'S':' ', attrh?'H':' ', attrr?'R':' ', uselfn?sfull:full);
			} else
				shell->WriteOut(MSG_Get("SHELL_CMD_ATTRIB_GET_ERROR"),uselfn?sfull:full);
		}
		res=DOS_FindNext();
	}
	if (optS) {
		size_t len=strlen(path);
		strcat(path, "*.*");
		bool ret=DOS_FindFirst(path,0xffff & ~DOS_ATTR_VOLUME);
		*(path+len)=0;
		if (ret) {
			std::vector<std::string> cdirs;
			cdirs.clear();
			do {    /* File name and extension */
				DtaResult result;
				dta.GetResult(result.name,result.lname,result.size,result.date,result.time,result.attr);

				if(result.attr&DOS_ATTR_DIRECTORY && strcmp(result.name, ".")&&strcmp(result.name, "..")) {
					strcat(path, result.name);
					strcat(path, "\\");
					char *fname = strrchr(args, '\\');
					if (fname!=NULL) fname++;
					else {
						fname = strrchr(args, ':');
						if (fname!=NULL) fname++;
						else fname=args;
					}
					strcat(path, fname);
					cdirs.push_back((path[0]!='"'&&path[strlen(path)-1]=='"'?"\"":"")+std::string(path));
					*(path+len)=0;
				}
			} while ( (ret=DOS_FindNext()) );
			adirs.insert(adirs.begin()+1, cdirs.begin(), cdirs.end());
		}
	}
	return found;
}

void DOS_Shell::CMD_ATTRIB(char *args){
	HELP("ATTRIB");
	StripSpaces(args);

	bool optS=ScanCMDBool(args,"S");
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	
	bool adda=false, adds=false, addh=false, addr=false, suba=false, subs=false, subh=false, subr=false;
	char sfull[DOS_PATHLENGTH+2];
	char* arg1;
	strcpy(sfull, "*.*");
	do {
		arg1=StripArg(args);
		if (!strcasecmp(arg1, "+A")) adda=true;
		else if (!strcasecmp(arg1, "+S")) adds=true;
		else if (!strcasecmp(arg1, "+H")) addh=true;
		else if (!strcasecmp(arg1, "+R")) addr=true;
		else if (!strcasecmp(arg1, "-A")) suba=true;
		else if (!strcasecmp(arg1, "-S")) subs=true;
		else if (!strcasecmp(arg1, "-H")) subh=true;
		else if (!strcasecmp(arg1, "-R")) subr=true;
		else if (*arg1) {
			strcpy(sfull, arg1);
			if (uselfn&&strchr(sfull, '*')) {
				char * find_last;
				find_last=strrchr(sfull,'\\');
				if (find_last==NULL) find_last=sfull;
				else find_last++;
				if (sfull[strlen(sfull)-1]=='*'&&strchr(find_last, '.')==NULL) strcat(sfull, ".*");
			}
		}
	} while (*args);

	char buffer[CROSS_LEN];
	args = ExpandDot(sfull,buffer, CROSS_LEN);
	StripSpaces(args);
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	adirs.clear();
	adirs.push_back(std::string(args));
	bool found=false;
	while (!adirs.empty()) {
		if (doAttrib(this, (char *)adirs.begin()->c_str(), dta, optS, adda, adds, addh, addr, suba, subs, subh, subr))
			found=true;
		adirs.erase(adirs.begin());
	}
	if (!found) WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
	dos.dta(save_dta);
}

void DOS_Shell::CMD_PROMPT(char *args){
	HELP("PROMPT");
	if(args && *args) {
		args++;
		SetEnv("PROMPT",args);
	} else
		SetEnv("PROMPT","$P$G");
	return;
}

void DOS_Shell::CMD_PATH(char *args){
	HELP("PATH");
	if(args && *args){
		char pathstring[DOS_PATHLENGTH+CROSS_LEN+20]={ 0 };
		strcpy(pathstring,"set PATH=");
		while(args && (*args=='='|| *args==' ')) 
		     args++;
        if (args)
            strcat(pathstring,args);
		this->ParseLine(pathstring);
		return;
	} else {
		std::string line;
		if(GetEnvStr("PATH",line)) {
			WriteOut("%s\n",line.c_str());
		} else {
			WriteOut("PATH=(null)\n");
		}
	}
}

void DOS_Shell::CMD_VERIFY(char * args) {
	HELP("VERIFY");
	args = trim(args);
	if (!*args)
		WriteOut("VERIFY is %s\n", dos.verify ? "on" : "off");
	else if (!strcasecmp(args, "OFF"))
		dos.verify = false;
	else if (!strcasecmp(args, "ON"))
		dos.verify = true;
	else
		WriteOut("Must specify ON or OFF\n");
}

void dos_ver_menu(bool start);
bool set_ver(char *s);
void DOS_Shell::CMD_VER(char *args) {
	HELP("VER");
	bool optR=ScanCMDBool(args,"R");
	if (char* rem = ScanCMDRemain(args)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), rem);
		return;
	}
	if(!optR && args && *args) {
		char* word = StripWord(args);
		if(strcasecmp(word,"set")) {
			if (isdigit(*word)) {
				if (*args) {
					WriteOut("Invalid parameter - %s\n", args);
					return;
				}
				if (set_ver(word)) {
					dos_ver_menu(false);
					return;
				}
			}
			WriteOut("Invalid parameter - %s\n", word);
			return;
		}
		if (!*args) {
			dos.version.major = 5;
			dos.version.minor = 0;
		} else if (!set_ver(args)) {
			WriteOut(MSG_Get("SHELL_CMD_VER_INVALID"));
			return;
		}
		dos_ver_menu(false);
	} else {
		WriteOut(MSG_Get("SHELL_CMD_VER_VER"),VERSION,SDL_STRING,dos.version.major,dos.version.minor);
		if (optR) WriteOut("DOSBox-X's build date and time: %s\n",UPDATED_STR);
	}
}

void DOS_Shell::CMD_VOL(char *args){
	HELP("VOL");
	Bit8u drive=DOS_GetDefaultDrive();
	if(args && *args){
		args++;
		Bit32u argLen = (Bit32u)strlen(args);
		switch (args[argLen-1]) {
		case ':' :
			if(!strcasecmp(args,":")) return;
			int drive2; drive2= toupper(*reinterpret_cast<unsigned char*>(&args[0]));
			char * c; c = strchr(args,':'); *c = '\0';
			if (Drives[drive2-'A']) drive = drive2 - 'A';
			else {
				WriteOut(MSG_Get("SHELL_CMD_VOL_DRIVEERROR"));
				return;
			}
			break;
		default:
			WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
			return;
		}
	}
	char const* bufin = Drives[drive]->GetLabel();
	WriteOut(MSG_Get("SHELL_CMD_VOL_DRIVE"),drive+'A');

	//if((drive+'A')=='Z') bufin="DOSBOX-X";
	if(strcasecmp(bufin,"")==0)
		WriteOut(MSG_Get("SHELL_CMD_VOL_SERIAL_NOLABEL"));
	else
		WriteOut(MSG_Get("SHELL_CMD_VOL_SERIAL_LABEL"),bufin);

	WriteOut(MSG_Get("SHELL_CMD_VOL_SERIAL"));
	unsigned long serial_number=0x1234;
	if (!strncmp(Drives[drive]->GetInfo(),"fatDrive ",9)) {
		fatDrive* fdp = dynamic_cast<fatDrive*>(Drives[drive]);
		if (fdp != NULL) serial_number=fdp->GetSerial();
	}
#if defined (WIN32)
	if (!strncmp(Drives[drive]->GetInfo(),"local ",6) || !strncmp(Drives[drive]->GetInfo(),"CDRom ",6)) {
		localDrive* ldp = !strncmp(Drives[drive]->GetInfo(),"local ",6)?dynamic_cast<localDrive*>(Drives[drive]):dynamic_cast<cdromDrive*>(Drives[drive]);
		if (ldp != NULL) serial_number=ldp->GetSerial();
	}
#endif
	WriteOut("%04X-%04X\n", serial_number/0x10000, serial_number%0x10000);
	return;
}

void DOS_Shell::CMD_TRUENAME(char * args) {
	HELP("TRUENAME");
	args = trim(args);
	if (!*args) {
		WriteOut("No file name given.\n");
		return;
	}
	if (char* rem = ScanCMDRemain(args)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), rem);
		return;
	}
	char *name = StripArg(args), fullname[DOS_PATHLENGTH];
	Bit8u drive;
	if (DOS_MakeName(name, fullname, &drive))
		WriteOut("%c:\\%s\r\n", drive+'A', fullname);
	else
		WriteOut(dos.errorcode==DOSERR_PATH_NOT_FOUND?"Path not found\n":"File not found\n");
}

void SetVal(const std::string& secname, const std::string& preval, const std::string& val);
static void delayed_press(Bitu key) { KEYBOARD_AddKey((KBD_KEYS)key,true); }
static void delayed_release(Bitu key) { KEYBOARD_AddKey((KBD_KEYS)key,false); }
static void delayed_sdlpress(Bitu core) {
	if(core==1) SetVal("cpu","core","normal");
	else if(core==2) SetVal("cpu","core","simple");
	else if(core==3) SetVal("cpu","core","dynamic");
	else if(core==4) SetVal("cpu","core","full");
}
// ADDKEY patch was created by Moe
void DOS_Shell::CMD_ADDKEY(char * args){
	HELP("ADDKEY");
	StripSpaces(args);
	if (!*args) {
		WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
		return;
	}
    pic_tickindex_t delay = 0;
    int duration = 0, core = 0;

	while (*args) {
		char *word=StripWord(args);
		KBD_KEYS scankey = (KBD_KEYS)0;
		char *tail;
		bool alt = false, ctrl = false, shift = false;
		while (word[1] == '-') {
			switch (word[0]) {
				case 'c':
					ctrl = true;
					word += 2;
					break;
				case 's':
					shift = true;
					word += 2;
					break;
				case 'a':
					alt = true;
					word += 2;
					break;
				default:
					WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
					return;
			}
		}
		if (!strcasecmp(word,"enter")) {
			word[0] = (char)10;
			word[1] = (char)0;
		} else if (!strcasecmp(word,"space")) {
			word[0] = (char)32;
			word[1] = (char)0;
		} else if (!strcasecmp(word,"bs")) {
			word[0] = (char)8;
			word[1] = (char)0;
		} else if (!strcasecmp(word,"tab")) {
			word[0] = (char)9;
			word[1] = (char)0;
		} else if (!strcasecmp(word,"escape")) {
			word[0] = (char)27;
			word[1] = (char)0;
		} else if (!strcasecmp(word,"up")) {
			word[0] = (char)141;
			word[1] = (char)0;
		} else if (!strcasecmp(word,"down")) {
			word[0] = (char)142;
			word[1] = (char)0;
		} else if (!strcasecmp(word,"left")) {
			word[0] = (char)143;
			word[1] = (char)0;
		} else if (!strcasecmp(word,"right")) {
			word[0] = (char)144;
			word[1] = (char)0;
		} else if (!strcasecmp(word,"ins")) {
			word[0] = (char)145;
			word[1] = (char)0;
		} else if (!strcasecmp(word,"del")) {
			word[0] = (char)146;
			word[1] = (char)0;
		} else if (!strcasecmp(word,"home")) {
			word[0] = (char)147;
			word[1] = (char)0;
		} else if (!strcasecmp(word,"end")) {
			word[0] = (char)148;
			word[1] = (char)0;
		} else if (!strcasecmp(word,"pgup")) {
			word[0] = (char)149;
			word[1] = (char)0;
		} else if (!strcasecmp(word,"pgdown")) {
			word[0] = (char)150;
			word[1] = (char)0;
		} else if (!strcasecmp(word,"normal")) {
			core = 1;
		} else if (!strcasecmp(word,"simple")) {
			core = 2;
		} else if (!strcasecmp(word,"dynamic")) {
			core = 3;
		} else if (!strcasecmp(word,"full")) {
			core = 4;
		} else if (word[0] == 'k' && word[1] == 'p' && word[2] && !word[3]) {
			word[0] = 151+word[2]-'0';
			word[1] = 0;
		} else if (word[0] == 'f' && word[1]) {
			word[0] = 128+word[1]-'0';
			if (word[1] == '1' && word[2]) word[0] = 128+word[2]-'0'+10;
			word[1] = 0;
		}
		if (!word[1]) {
			const int shiftflag = 0x1000000;
			const int map[256] = {
				0,0,0,0,0,0,0,0,
				KBD_backspace,
				KBD_tab,
				KBD_enter,
				0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				KBD_esc,
				0,0,0,0,
				KBD_space, KBD_1|shiftflag, KBD_quote|shiftflag, KBD_3|shiftflag, KBD_4|shiftflag, KBD_5|shiftflag, KBD_7|shiftflag, KBD_quote,
				KBD_9|shiftflag, KBD_0|shiftflag, KBD_8|shiftflag, KBD_equals|shiftflag, KBD_comma, KBD_minus, KBD_period, KBD_slash,
				KBD_0, KBD_1, KBD_2, KBD_3, KBD_4, KBD_5, KBD_6, KBD_7,
				KBD_8, KBD_9, KBD_semicolon|shiftflag, KBD_semicolon, KBD_comma|shiftflag, KBD_equals, KBD_period|shiftflag, KBD_slash|shiftflag,
				KBD_2|shiftflag, KBD_a|shiftflag, KBD_b|shiftflag, KBD_c|shiftflag, KBD_d|shiftflag, KBD_e|shiftflag, KBD_f|shiftflag, KBD_g|shiftflag,
				KBD_h|shiftflag, KBD_i|shiftflag, KBD_j|shiftflag, KBD_k|shiftflag, KBD_l|shiftflag, KBD_m|shiftflag, KBD_n|shiftflag, KBD_o|shiftflag,
				KBD_p|shiftflag, KBD_q|shiftflag, KBD_r|shiftflag, KBD_s|shiftflag, KBD_t|shiftflag, KBD_u|shiftflag, KBD_v|shiftflag, KBD_w|shiftflag,
				KBD_x|shiftflag, KBD_y|shiftflag, KBD_z|shiftflag, KBD_leftbracket, KBD_backslash, KBD_rightbracket, KBD_6|shiftflag, KBD_minus|shiftflag,
				KBD_grave, KBD_a, KBD_b, KBD_c, KBD_d, KBD_e, KBD_f, KBD_g,
				KBD_h, KBD_i, KBD_j, KBD_k, KBD_l, KBD_m, KBD_n, KBD_o,
				KBD_p, KBD_q, KBD_r, KBD_s, KBD_t, KBD_u, KBD_v, KBD_w,
				KBD_x, KBD_y, KBD_z, KBD_leftbracket|shiftflag, KBD_backslash|shiftflag, KBD_rightbracket|shiftflag, KBD_grave|shiftflag, 0,
				0, KBD_f1, KBD_f2, KBD_f3, KBD_f4, KBD_f5, KBD_f6, KBD_f7, KBD_f8, KBD_f9, KBD_f10, KBD_f11, KBD_f12,
				KBD_up, KBD_down, KBD_left, KBD_right, KBD_insert, KBD_delete, KBD_home, KBD_end, KBD_pageup, KBD_pagedown,
				KBD_kp0, KBD_kp1, KBD_kp2, KBD_kp3, KBD_kp4, KBD_kp5, KBD_kp6, KBD_kp7, KBD_kp8, KBD_kp9, 
			};
			scankey = (KBD_KEYS)(map[(unsigned char)word[0]] & ~shiftflag);
			if (map[(unsigned char)word[0]] & shiftflag) shift = true;
			if (!scankey && core == 0) {
				WriteOut(MSG_Get("SHELL_SYNTAXERROR"),word);
				return;
			}
			if (core == 0) word[0] = 0;
		}
		if (word[0] == 'p') {
			delay += strtol(word+1,&tail,0);
			if (tail && *tail) {
				WriteOut(MSG_Get("SHELL_SYNTAXERROR"),word);
				return;
			}
		} else if (word[0] == 'l') {
			duration = strtol(word+1,&tail,0);
			if (tail && *tail) {
				WriteOut(MSG_Get("SHELL_SYNTAXERROR"),word);
				return;
			}
		} else if (!word[0] || ((scankey = (KBD_KEYS)strtol(word,NULL,0)) > KBD_NONE && scankey < KBD_LAST)) {
			if (shift) {
				if (delay == 0) KEYBOARD_AddKey(KBD_leftshift,true);
				else PIC_AddEvent(&delayed_press,delay++,KBD_leftshift);
			}
			if (ctrl) {
				if (delay == 0) KEYBOARD_AddKey(KBD_leftctrl,true);
				else PIC_AddEvent(&delayed_press,delay++,KBD_leftctrl);
			}
			if (alt) {
				if (delay == 0) KEYBOARD_AddKey(KBD_leftalt,true);
			else PIC_AddEvent(&delayed_press,delay++,KBD_leftalt);
			}
			if (delay == 0) KEYBOARD_AddKey(scankey,true);
			else PIC_AddEvent(&delayed_press,delay++,scankey);

			if (delay+duration == 0) KEYBOARD_AddKey(scankey,false);
			else PIC_AddEvent(&delayed_release,delay+++duration,scankey);
			if (alt) {
				if (delay+duration == 0) KEYBOARD_AddKey(KBD_leftalt,false);
				else PIC_AddEvent(&delayed_release,delay+++duration,KBD_leftalt);
			}
			if (ctrl) {
				if (delay+duration == 0) KEYBOARD_AddKey(KBD_leftctrl,false);
				else PIC_AddEvent(&delayed_release,delay+++duration,KBD_leftctrl);
			}
			if (shift) {
				if (delay+duration == 0) KEYBOARD_AddKey(KBD_leftshift,false);
				else PIC_AddEvent(&delayed_release,delay+++duration,KBD_leftshift);
			}
		} else if (core != 0) {
			if (core == 1) {
				if (delay == 0) SetVal("cpu","core","normal");
				else PIC_AddEvent(&delayed_sdlpress,delay++,1);
			} else if (core == 2) {
				if (delay == 0) SetVal("cpu","core","simple");
				else PIC_AddEvent(&delayed_sdlpress,delay++,2);
			} else if (core == 3) {
				if (delay == 0) SetVal("cpu","core","dynamic");
				else PIC_AddEvent(&delayed_sdlpress,delay++,3);
			} else if (core == 4) {
				if (delay == 0) SetVal("cpu","core","full");
				else PIC_AddEvent(&delayed_sdlpress,delay++,4);
			}
		} else {
			WriteOut(MSG_Get("SHELL_SYNTAXERROR"),word);
			return;
		}
	}
 }

#if C_DEBUG
bool debugger_break_on_exec = false;

void DOS_Shell::CMD_DEBUGBOX(char * args) {
    while (*args == ' ') args++;
	std::string argv=std::string(args);
	args=StripArg(args);
	HELP("DEBUGBOX");
    /* TODO: The command as originally taken from DOSBox SVN supported a /NOMOUSE option to remove the INT 33h vector */
    debugger_break_on_exec = true;
    if (!strcmp(args,"-?")) {
		args[0]='/';
		HELP("DEBUGBOX");
		return;
	}
    DoCommand((char *)argv.c_str());
    debugger_break_on_exec = false;
}
#endif

static char *str_replace(char *orig, char *rep, char *with) {
    char *result, *ins, *tmp;
    size_t len_rep, len_with, len_front;
    int count;

    if (!orig || !rep) return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0) return NULL;
    len_with = with?strlen(with):0;

    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)) != NULL; ++count)
        ins = tmp + len_rep;

    tmp = result = (char *)malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result) return NULL;
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with?with:"") + len_with;
        orig += len_front + len_rep;
    }
    strcpy(tmp, orig);
    return result;
}

void DOS_Shell::CMD_FOR(char *args) {
	HELP("FOR");
	args = ltrim(args);
	if (strlen(args)<12){SyntaxError();return;}
	char s[3];
	strcpy(s,"%%");
	if (*args=='%' && (isalpha(args[1]) || isdigit(args[1]) || strchr("_-/*.;#$",args[1])) && isspace(args[2]))
		s[1]=*(args+1);
	else{SyntaxError();return;}
	args = ltrim(args+3);
	if (strncasecmp(args, "IN", 2) || !isspace(args[2])){SyntaxError();return;}
	args = ltrim(args+3);
	if (*args=='(')
		args = ltrim(args+1);
	else{SyntaxError();return;}
	char *p=strchr(args, ')');
	if (p==NULL||!isspace(*(p+1))){SyntaxError();return;}
	*p=0;
	char flist[260], *fp=flist;
	if (strlen(ltrim(args))<260)
		strcpy(flist, ltrim(args));
	else
		{
		strncpy(flist, args, 259);
		flist[259]=0;
		}
	*p=')';
	args=ltrim(p+2);
	if (strncasecmp(args, "DO", 2) || !isspace(args[2])){SyntaxError();return;}
	args = ltrim(args+3);
	bool lfn=uselfn&&lfnfor;
	while (*fp) {
		p=fp;
		int q=0;
		while (*p&&(q/2*2!=q||(*p!=' '&&*p!=','&&*p!=';')))
			{
			if (*p=='"')
				q++;
			p++;
			}
		bool last=!!strlen(p);
		if (last) *p=0;
		if (strchr(fp, '?') || strchr(fp, '*')) {
			char name[DOS_NAMELENGTH_ASCII], lname[LFN_NAMELENGTH], spath[DOS_PATHLENGTH], path[DOS_PATHLENGTH], pattern[DOS_PATHLENGTH], full[DOS_PATHLENGTH], *r;
			if (!DOS_Canonicalize(fp,full)) return;
			r=strrchr(full, '\\');
			if (r!=NULL) {
				*r=0;
				strcpy(path, full);
				strcat(path, "\\");
				strcpy(pattern, r+1);
				*r='\\';
			} else {
				strcpy(path, "");
				strcpy(pattern, full);
			}
			strcpy(spath, path);
			if (strchr(fp,'\"')||uselfn) {
				if (!DOS_GetSFNPath(("\""+std::string(path)+"\\").c_str(), spath, false)) strcpy(spath, path);
				if (!strlen(spath)||spath[strlen(spath)-1]!='\\') strcat(spath, "\\");
				int k=0;
				for (int i=0;i<(int)strlen(path);i++)
					if (path[i]!='\"')
						path[k++]=path[i];
				path[k]=0;
			}
			Bit32u size;
			Bit16u date, time;
			Bit8u attr;
			DOS_DTA dta(dos.dta());
			std::vector<std::string> sources;
			std::string tmp;
			int fbak=lfn_filefind_handle;
			lfn_filefind_handle=lfn?LFN_FILEFIND_INTERNAL:LFN_FILEFIND_NONE;
			if (DOS_FindFirst((char *)(std::string(spath)+std::string(pattern)).c_str(), ~(DOS_ATTR_VOLUME|DOS_ATTR_DIRECTORY|DOS_ATTR_DEVICE|DOS_ATTR_HIDDEN|DOS_ATTR_SYSTEM)))
				{
				dta.GetResult(name, lname, size, date, time, attr);
				tmp=std::string(path)+std::string(lfn?lname:name);
				sources.push_back(tmp);
				while (DOS_FindNext())
					{
					dta.GetResult(name, lname, size, date, time, attr);
					tmp=std::string(path)+std::string(lfn?lname:name);
					sources.push_back(tmp);
					}
				}
			lfn_filefind_handle=fbak;
			for (std::vector<std::string>::iterator source = sources.begin(); source != sources.end(); ++source)
				DoCommand(str_replace(args, s, (char *)source->c_str()));
		} else
			DoCommand(str_replace(args, s, fp));
		if (last) *p=' ';
		fp=ltrim(p);
	}
}

void DOS_Shell::CMD_LFNFOR(char * args) {
	HELP("LFNFOR");
	args = trim(args);
	if (!*args)
		WriteOut("LFNFOR is %s\n", lfnfor ? "on" : "off");
	else if (!strcasecmp(args, "OFF"))
		lfnfor = false;
	else if (!strcasecmp(args, "ON"))
		lfnfor = true;
	else
		WriteOut("Must specify ON or OFF\n");
}

void DOS_Shell::CMD_ALIAS(char* args) {
    HELP("ALIAS");
	args = trim(args);
    if (!*args || strchr(args, '=') == NULL) {
        for (cmd_alias_map_t::iterator iter = cmd_alias.begin(), end = cmd_alias.end();
            iter != end; ++iter) {
			if (!*args || !strcasecmp(args, iter->first.c_str()))
				WriteOut("ALIAS %s='%s'\n", iter->first.c_str(), iter->second.c_str());
        }
    } else {
        char alias_name[256] = { 0 };
        char* cmd = 0;
        for (unsigned int offset = 0; *args && offset < sizeof(alias_name)-1; ++offset, ++args) {
            if (*args == '=') {
                cmd = trim(alias_name);
                ++args;
                args = trim(args);
                size_t args_len = strlen(args);
                if ((*args == '"' && args[args_len - 1] == '"') || (*args == '\'' && args[args_len - 1] == '\'')) {
                    args[args_len - 1] = 0;
                    ++args;
                }
                if (!*args) {
                    cmd_alias.erase(cmd);
                } else {
                    cmd_alias[cmd] = args;
                }
                break;
            } else {
                alias_name[offset] = *args;
            }
        }
    }
}

void CAPTURE_StartCapture(void);
void CAPTURE_StopCapture(void);

void CAPTURE_StartWave(void);
void CAPTURE_StopWave(void);

void CAPTURE_StartMTWave(void);
void CAPTURE_StopMTWave(void);

// Explanation: Start capture, run program, stop capture when program exits.
//              Great for gameplay footage or demoscene capture.
//
//              The command name is chosen not to conform to the 8.3 pattern
//              on purpose to avoid conflicts with any existing DOS applications.
void DOS_Shell::CMD_DXCAPTURE(char * args) {
    while (*args == ' ') args++;
	std::string argv=std::string(args);
	args=StripArg(args);
	HELP("DXCAPTURE");
    bool cap_video = false;
    bool cap_audio = false;
    bool cap_mtaudio = false;
    unsigned long post_exit_delay_ms = 3000; /* 3 sec */

    if (!strcmp(args,"-?")) {
		args[0]='/';
		HELP("DXCAPTURE");
		return;
	}

    args=(char *)argv.c_str();
    char *arg1;
    while (strlen(args)&&args[0]=='/') {
		arg1=StripArg(args);
		upcase(arg1);
		if (!(strcmp(arg1,"/V")))
			cap_video = true;
		else if (!(strcmp(arg1,"/-V")))
			cap_video = false;
		else if (!(strcmp(arg1,"/A")))
			cap_audio = true;
		else if (!(strcmp(arg1,"/-A")))
			cap_audio = false;
		else if (!(strcmp(arg1,"/M")))
			cap_mtaudio = true;
		else if (!(strcmp(arg1,"/-M")))
			cap_mtaudio = false;
		else {
			WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),arg1);
			return;
		}
    }

    if (!cap_video && !cap_audio && !cap_mtaudio)
        cap_video = true;

    if (cap_video)
        CAPTURE_StartCapture();
    if (cap_audio)
        CAPTURE_StartWave();
    if (cap_mtaudio)
        CAPTURE_StartMTWave();

    DoCommand(args);

    if (post_exit_delay_ms > 0) {
        LOG_MSG("Pausing for post exit delay (%.3f seconds)",(double)post_exit_delay_ms / 1000);

        Bit32u lasttick=GetTicks();
        while ((GetTicks()-lasttick)<post_exit_delay_ms) {
            CALLBACK_Idle();

            if (machine == MCH_PC98) {
                reg_eax = 0x0100;   // sense key
                CALLBACK_RunRealInt(0x18);
                SETFLAGBIT(ZF,reg_bh == 0);
            }
            else {
                reg_eax = 0x0100;
                CALLBACK_RunRealInt(0x16);
            }

            if (!GETFLAG(ZF)) {
                if (machine == MCH_PC98) {
                    reg_eax = 0x0000;   // read key
                    CALLBACK_RunRealInt(0x18);
                }
                else {
                    reg_eax = 0x0000;
                    CALLBACK_RunRealInt(0x16);
                }

                if (reg_al == 32/*space*/ || reg_al == 27/*escape*/)
                    break;
            }
        }
    }

    if (cap_video)
        CAPTURE_StopCapture();
    if (cap_audio)
        CAPTURE_StopWave();
    if (cap_mtaudio)
        CAPTURE_StopMTWave();
}

void DOS_Shell::CMD_CTTY(char * args) {
	HELP("CTTY");
	/* NTS: This is written to emulate the simplistic parsing in MS-DOS 6.22 */
	Bit16u handle;
	int i;

	/* args has leading space? */
	args = trim(args);

	/* must be device */
	if (DOS_FindDevice(args) == DOS_DEVICES) {
		WriteOut("Invalid device");
		return;
	}

	/* close STDIN/STDOUT/STDERR and replace with new handle */
	if (!DOS_OpenFile(args,OPEN_READWRITE,&handle)) {
		WriteOut("Unable to open device");
		return;
	}

	for (i=0;i < 3;i++) {
		DOS_CloseFile(i);
		DOS_ForceDuplicateEntry(handle,i);
	}
	DOS_CloseFile(handle);
}

void DOS_Shell::CMD_COUNTRY(char * args) {
	HELP("COUNTRY");
	if (char* rem = ScanCMDRemain(args))
		{
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), rem);
		return;
		}
	args = trim(args);
	if (!*args)
		{
		WriteOut("Current country code: %d\r\n", countryNo);
		return;
		}
	int newCC;
	char buffer[256];
	if (sscanf(args, "%d%s", &newCC, buffer) == 1 && newCC>0)
		{
		countryNo = newCC;
		DOS_SetCountry(countryNo);
		return;
		}
	WriteOut("Invalid country code\r\n");
	return;
}

