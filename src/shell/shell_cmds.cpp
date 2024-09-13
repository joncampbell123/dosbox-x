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
 *
 *  Heavy improvements by the DOSBox-X Team, 2011-2021
 *  DX-CAPTURE, DEBUGBOX, INT2FDBG commands by joncampbell123
 *  ATTRIB, CHCP, COUNTRY, DELTREE, FOR/LFNFOR, POPD/PUSHD, TREE, TRUENAME, VERIFY commands by Wengier
 *  LS command by the DOSBox Staging Team and Wengier
 */


#include "dosbox.h"
#include "logging.h"
#include "shell.h"
#include "callback.h"
#include "dos_inc.h"
#include "regs.h"
#include "pic.h"
#include "keyboard.h"
#include "timer.h"
#include "../ints/int10.h"
#include <time.h>
#include <assert.h>
#include "bios.h"
#include "../dos/drives.h"
#include "support.h"
#include "control.h"
#include "paging.h"
#include "menu.h"
#include "jfont.h"
#include "render.h"
#include <algorithm>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <vector>
#include <string>
#include "sdlmain.h"
#include "menudef.h"
#include "build_timestamp.h"
#include "version_string.h"

#include <output/output_ttf.h>

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

SHELL_Cmd cmd_list[]={
{	"DIR",			0,		&DOS_Shell::CMD_DIR,		"SHELL_CMD_DIR_HELP"},
{	"CD",			0,		&DOS_Shell::CMD_CHDIR,		"SHELL_CMD_CHDIR_HELP"},
{	"ALIAS",		1,		&DOS_Shell::CMD_ALIAS,		"SHELL_CMD_ALIAS_HELP"},
{	"ASSOC",		1,		&DOS_Shell::CMD_ASSOC,		"SHELL_CMD_ASSOC_HELP"},
{	"ATTRIB",		1,		&DOS_Shell::CMD_ATTRIB,		"SHELL_CMD_ATTRIB_HELP"},
{	"BREAK",		1,		&DOS_Shell::CMD_BREAK,		"SHELL_CMD_BREAK_HELP"},
{	"CALL",			1,		&DOS_Shell::CMD_CALL,		"SHELL_CMD_CALL_HELP"},
{	"CHDIR",		1,		&DOS_Shell::CMD_CHDIR,		"SHELL_CMD_CHDIR_HELP"},
//{	"CHOICE",		1,		&DOS_Shell::CMD_CHOICE,		"SHELL_CMD_CHOICE_HELP"}, // CHOICE as a program (Z:\DOS\CHOICE.COM) instead of shell command
{	"CLS",			0,		&DOS_Shell::CMD_CLS,		"SHELL_CMD_CLS_HELP"},
{	"COPY",			0,		&DOS_Shell::CMD_COPY,		"SHELL_CMD_COPY_HELP"},
{	"CHCP",			1,		&DOS_Shell::CMD_CHCP,		"SHELL_CMD_CHCP_HELP"},
//{	"COUNTRY",		1,		&DOS_Shell::CMD_COUNTRY,	"SHELL_CMD_COUNTRY_HELP"}, // COUNTRY as a program (Z:\SYSTEM\COUNTRY.COM) instead of shell command
{	"CTTY",			1,		&DOS_Shell::CMD_CTTY,		"SHELL_CMD_CTTY_HELP"},
{	"DATE",			0,		&DOS_Shell::CMD_DATE,		"SHELL_CMD_DATE_HELP"},
{	"DEL",			0,		&DOS_Shell::CMD_DELETE,		"SHELL_CMD_DELETE_HELP"},
//{	"DELTREE",		1,		&DOS_Shell::CMD_DELTREE,	"SHELL_CMD_DELTREE_HELP"}, // DELTREE as a program (Z:\DOS\DELTREE.EXE) instead of shell command
{	"ECHO",			0,		&DOS_Shell::CMD_ECHO,		"SHELL_CMD_ECHO_HELP"},
{	"ERASE",		1,		&DOS_Shell::CMD_DELETE,		"SHELL_CMD_DELETE_HELP"},
{	"EXIT",			0,		&DOS_Shell::CMD_EXIT,		"SHELL_CMD_EXIT_HELP"},
{	"FOR",			1,		&DOS_Shell::CMD_FOR,		"SHELL_CMD_FOR_HELP"},
{	"GOTO",			1,		&DOS_Shell::CMD_GOTO,		"SHELL_CMD_GOTO_HELP"},
//{	"HELP",			1,		&DOS_Shell::CMD_HELP,		"SHELL_CMD_HELP_HELP"}, // HELP as a program (Z:\SYSTEM\HELP.COM) instead of shell command
{	"HISTORY",		1,		&DOS_Shell::CMD_HISTORY,	"SHELL_CMD_HISTORY_HELP"},
{	"IF",			1,		&DOS_Shell::CMD_IF,			"SHELL_CMD_IF_HELP"},
{	"LFNFOR",		1,		&DOS_Shell::CMD_LFNFOR,		"SHELL_CMD_LFNFOR_HELP"},
{	"LH",			1,		&DOS_Shell::CMD_LOADHIGH,	"SHELL_CMD_LOADHIGH_HELP"},
{	"LOADHIGH",		1,		&DOS_Shell::CMD_LOADHIGH,	"SHELL_CMD_LOADHIGH_HELP"},
//{   "LS",			1,		&DOS_Shell::CMD_LS,			"SHELL_CMD_LS_HELP"}, // LS as a program (Z:\BIN\LS.COM) instead of shell command
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
{	"TRUENAME",		1,		&DOS_Shell::CMD_TRUENAME,	"SHELL_CMD_TRUENAME_HELP"},
{	"TYPE",			0,		&DOS_Shell::CMD_TYPE,		"SHELL_CMD_TYPE_HELP"},
{	"VER",			0,		&DOS_Shell::CMD_VER,		"SHELL_CMD_VER_HELP"},
{	"VERIFY",		1,		&DOS_Shell::CMD_VERIFY,		"SHELL_CMD_VERIFY_HELP"},
{	"VOL",			0,		&DOS_Shell::CMD_VOL,		"SHELL_CMD_VOL_HELP"},
{	"POPD",			1,		&DOS_Shell::CMD_POPD,		"SHELL_CMD_POPD_HELP"},
{	"PUSHD",		1,		&DOS_Shell::CMD_PUSHD,		"SHELL_CMD_PUSHD_HELP"},
#if C_DEBUG
// Additional commands for debugging purposes in DOSBox-X
{	"DEBUGBOX",		1,		&DOS_Shell::CMD_DEBUGBOX,	"SHELL_CMD_DEBUGBOX_HELP"},
//{	"INT2FDBG",		1,		&DOS_Shell::CMD_INT2FDBG,	"SHELL_CMD_INT2FDBG_HELP"}, // INT2FDBG as a program (Z:\DEBUG\INT2FDBG.COM) instead of shell command
#endif
// Advanced commands specific to DOSBox-X
//{	"ADDKEY",		1,		&DOS_Shell::CMD_ADDKEY,		"SHELL_CMD_ADDKEY_HELP"}, // ADDKEY as a program (Z:\BIN\ADDKEY.COM) instead of shell command
{	"DX-CAPTURE",	1,		&DOS_Shell::CMD_DXCAPTURE,  "SHELL_CMD_DXCAPTURE_HELP"},
{ nullptr, 0, nullptr, nullptr }
};

const char *GetCmdName(int i) {
    size_t n = sizeof(cmd_list)/sizeof(cmd_list[0])-1;
    return i>n?NULL:cmd_list[i].name;
}

extern int enablelfn, lfn_filefind_handle, file_access_tries, lastmsgcp;
extern bool date_host_forced, usecon, outcon, rsize, autoboxdraw, dbcs_sbcs, sync_time, manualtime, inshell, noassoc, dotype, loadlang;
extern unsigned long freec;
extern uint8_t DOS_GetAnsiAttr(void);
extern uint16_t countryNo, altcp_to_unicode[256];
extern bool isDBCSCP(), isKanji1(uint8_t chr), shiftjis_lead_byte(int c), TTF_using(void), Network_IsNetworkResource(const char * filename);
extern bool CheckBoxDrawing(uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4), GFX_GetPreventFullscreen(void), DOS_SetAnsiAttr(uint8_t attr);
extern bool systemmessagebox(char const * aTitle, char const * aMessage, char const * aDialogType, char const * aIconType, int aDefaultButton);
extern void Load_Language(std::string name), SwitchLanguage(int oldcp, int newcp, bool confirm), GetExpandedPath(std::string &path);
extern void MAPPER_AutoType(std::vector<std::string> &sequence, const uint32_t wait_ms, const uint32_t pace_ms, bool choice);
extern void DOS_SetCountry(uint16_t countryNo), DOSV_FillScreen(void);
std::string GetDOSBoxXPath(bool withexe=false);
FILE *testLoadLangFile(const char *fname);

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

static char* ExpandDot(char*args, char* buffer , size_t bufsize, bool expand) {
	if(*args == '.') {
		if(*(args+1) == 0){
			safe_strncpy(buffer, "*.*", bufsize);
			return buffer;
		}
		if( (*(args+1) != '.') && (*(args+1) != '\\') && expand) {
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

static char* ExpandDotMore(char*args, char* buffer , size_t bufsize) {
	char * find_last;
	find_last=strrchr_dbcs(args,'\\');
	if (find_last!=NULL) find_last++;
	if (find_last!=NULL && *find_last == '.') {
		if(*(find_last+1) == 0){
			safe_strncpy(buffer, args, bufsize);
			return buffer;
		}
		if( (*(find_last+1) != '.')) {
			*find_last = 0;
			strcpy(buffer, args);
			*find_last = '.';
			size_t len = strlen(buffer);
			buffer[len] = '*';
			buffer[len+1] = 0;
			if (bufsize > len + 2) strncat(buffer,find_last,bufsize - len - 1 /*used buffer portion*/ - 1 /*trailing zero*/  );
			else safe_strncpy(buffer, args, bufsize);
			return buffer;
		} else
			safe_strncpy(buffer, args, bufsize);
	}
	else safe_strncpy(buffer, args, bufsize);
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
	char newcom[1024]; newcom[0] = 0; strcpy(newcom,"z:\\system\\config -set ");
	if (line != NULL) {
		line=trim(line);
		if (*line=='=') line=trim(++line);
		if (line[0]=='"'&&line[strlen(line)-1]=='"') {
			line[strlen(line)-1]=0;
			line++;
			quote=true;
		}
		if (quote) strcat(newcom,"\"");
		strcat(newcom,test->GetName());	strcat(newcom," ");
		strcat(newcom,cmd_in);
		strcat(newcom, "=");
		strcat(newcom, line);
		if (quote) strcat(newcom,"\"");
	} else
		E_Exit("'line' in CheckConfig is NULL");
	DoCommand(newcom);
	return true;
}

bool enable_config_as_shell_commands = false;

bool DOS_Shell::execute_shell_cmd(char *name, char *arguments) {
	// SHELL_Cmd shell_cmd = {}; /* unused */
	uint32_t cmd_index=0;
	while (cmd_list[cmd_index].name) {
		if (strcasecmp(cmd_list[cmd_index].name,name)==0) {
			(this->*(cmd_list[cmd_index].handler))(arguments);
			return true;
		}
		cmd_index++;
	}
	return false;
}

void DOS_Shell::DoCommand(char * line) {
	/* First split the line into command and arguments */
	std::string origin_cmd_line = line;
	std::string last_alias_cmd;
	std::string altered_cmd_line;
	int alias_counter = 0;
__do_command_begin:
	if (alias_counter > 64) {
		WriteOut(MSG_Get("SHELL_EXECUTE_ALIAS_EXPAND_OVERFLOW"), origin_cmd_line.c_str());
	}
	line=trim(line);
	char cmd_buffer[CMD_MAXLINE];
	char * cmd_write=cmd_buffer;
	int c=0,q=0;
	while (*line) {
		if (*line == '/' || *line == '\t') break;

		if ((q & 1) == 0) {
			if (*line == ' ' || *line == '=') break;

			if (*line == '.' || *line == ';' || (*line == ':' && !(c == 1 && tolower(*(line-1)) >= 'a' && tolower(*(line-1)) <= 'z')) || *line == '[' || *line == ']' ||	*line == '\\' || *line == '/' || *line == '\"' || *line == '+') {  //allow stuff like cd.. and dir.exe cd\kees
				*cmd_write=0;
				if (execute_shell_cmd(cmd_buffer,line)) return;
			}
		}
		c++;
		if (*line == '"') q++;
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
	if (execute_shell_cmd(cmd_buffer,line)) return;

	/* This isn't an internal command execute it */
	char ldir[CROSS_LEN], *p=ldir;
	if (strchr(cmd_buffer,'\"')&&DOS_GetSFNPath(cmd_buffer,ldir,false)) {
		if (!strchr_dbcs(cmd_buffer, '\\') && strrchr_dbcs(ldir, '\\'))
			p=strrchr_dbcs(ldir, '\\')+1;
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
	std::string errhandler = static_cast<Section_prop *>(control->GetSection("dos"))->Get_string("badcommandhandler");
	if (errhandler.size()&&!noassoc) {
		noassoc=true;
		LOG_MSG("errhandler %s line %s\n", errhandler.c_str(), origin_cmd_line.c_str());
		DoCommand((char *)(errhandler+" "+origin_cmd_line).c_str());
		noassoc=false;
	} else
		WriteOut(MSG_Get("SHELL_EXECUTE_ILLEGAL_COMMAND"),cmd_buffer);
}

#define HELP(command) \
	if (ScanCMDBool(args,"?")) { \
		uint8_t attr = DOS_GetAnsiAttr(); \
		WriteOut(MSG_Get("SHELL_CMD_" command "_HELP")); \
		const char* long_m = MSG_Get("SHELL_CMD_" command "_HELP_LONG"); \
		WriteOut("\n"); \
		if(strcmp("Message not Found!\n",long_m)) WriteOut(long_m); \
		else WriteOut(command "\n"); \
		if (attr) DOS_SetAnsiAttr(attr); \
		return; \
	}

#if C_DEBUG
extern Bitu int2fdbg_hook_callback;
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

void Int2fhook() {
    uint32_t old_int2Fh;
    PhysPt w;

    int2fdbg_hook_callback = CALLBACK_Allocate();
    CALLBACK_Setup(int2fdbg_hook_callback,&INT2FDBG_Handler,CB_IRET,"INT 2Fh DBG callback");

    /* record old vector, set our new vector */
    old_int2Fh = RealGetVec(0x2f);
    w = CALLBACK_PhysPointer(int2fdbg_hook_callback);
    RealSetVec(0x2f,CALLBACK_RealPointer(int2fdbg_hook_callback));

    /* overwrite the callback with code to chain the call down, then invoke our callback on the way back up: */

    /* first, chain to the previous INT 15h handler */
    phys_writeb(w++,(uint8_t)0x9C);					//PUSHF
    phys_writeb(w++,(uint8_t)0x9A);					//CALL FAR <address>
    phys_writew(w,(uint16_t)(old_int2Fh&0xFFFF)); w += 2;		//offset
    phys_writew(w,(uint16_t)((old_int2Fh>>16)&0xFFFF)); w += 2;	//seg

    /* then, having returned from it, invoke our callback */
    phys_writeb(w++,(uint8_t)0xFE);					//GRP 4
    phys_writeb(w++,(uint8_t)0x38);					//Extra Callback instruction
    phys_writew(w,(uint16_t)int2fdbg_hook_callback); w += 2;		//The immediate word

    /* return */
    phys_writeb(w++,(uint8_t)0xCF);					//IRET
}

/* NTS: I know I could just modify the DOS kernel's INT 2Fh code to receive the init call,
 *      the problem is that at that point, the registers do not yet contain anything interesting.
 *      all the interesting results of the call are added by TSRs on the way back UP the call
 *      chain. The purpose of this program therefore is to hook INT 2Fh on the other end
 *      of the call chain so that we can see the results just before returning INT 2Fh back
 *      to WIN.COM */
void DOS_Shell::CMD_INT2FDBG(char * args) {
	//HELP("INT2FDBG");
    while (*args == ' ') args++;

	/* TODO: Allow /U to remove INT 2Fh hook */
	if (ScanCMDBool(args,"I")) {
		if (int2fdbg_hook_callback == 0) {
			Int2fhook();
			LOG_MSG("INT 2Fh debugging hook set\n");
			WriteOut("INT 2Fh hook has been set.\n");
		} else
			WriteOut("INT 2Fh hook was already set up.\n");
	} else if (*args)
		WriteOut(MSG_Get("SHELL_INVALID_PARAMETER"), args);
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

bool is_ANSI_installed(Program *shell);
void DOS_Shell::CMD_CLS(char * args) {
	HELP("CLS");
   if ((CurMode->type==M_TEXT || IS_PC98_ARCH) && is_ANSI_installed(this))
       WriteOut("\033[2J");
   else {
      uint16_t oldax=reg_ax;
      if (IS_DOSV && DOSV_CheckCJKVideoMode()) reg_ax = GetTrueVideoMode();
      else reg_ax=(uint16_t)CurMode->mode;
      CALLBACK_RunRealInt(0x10);
      reg_ax=oldax;
   } 
}

void DOS_Shell::CMD_DELETE(char* args) {
    HELP("DELETE");
    bool optP = ScanCMDBool(args, "P");
    bool optF = ScanCMDBool(args, "F");
    bool optQ = ScanCMDBool(args, "Q");

    const char ch_y = MSG_Get("INT21_6523_YESNO_CHARS")[0];
    const char ch_n = MSG_Get("INT21_6523_YESNO_CHARS")[1];
    const char ch_Y = toupper(ch_y);
    const char ch_N = toupper(ch_n);

    // ignore /f, /s, /ar, /as, /ah and /aa switches for compatibility
    ScanCMDBool(args, "S");
    ScanCMDBool(args, "AR");
    ScanCMDBool(args, "AS");
    ScanCMDBool(args, "AH");
    ScanCMDBool(args, "AA");

    char* rem = ScanCMDRemain(args);
    if(rem) {
        WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), rem);
        return;
    }
    if(!*args) {
        WriteOut(MSG_Get("SHELL_MISSING_PARAMETER"));
        return;
    }

    StripSpaces(args);
    args = trim(args);

    /* Command uses dta so set it to our internal dta */
    //DOS_DTA dta(dos.dta());
    RealPt save_dta = dos.dta();
    dos.dta(dos.tables.tempdta);
    DOS_DTA dta(dos.dta());
    /* If delete accept switches mind the space in front of them. See the dir /p code */

    char full[DOS_PATHLENGTH], sfull[DOS_PATHLENGTH + 2];
    char buffer[CROSS_LEN];
    char name[DOS_NAMELENGTH_ASCII], lname[LFN_NAMELENGTH + 1];
    uint32_t size, hsize; uint16_t time, date; uint8_t attr;
    args = ExpandDot(args, buffer, CROSS_LEN, false);
    StripSpaces(args);
    if(!DOS_Canonicalize(args, full)) { WriteOut(MSG_Get("SHELL_ILLEGAL_PATH")); dos.dta(save_dta); return; }
    if(strlen(args) && args[strlen(args) - 1] != '\\') {
        uint16_t fattr;
        if(strcmp(args, "*.*") && DOS_GetFileAttr(args, &fattr) && (fattr & DOS_ATTR_DIRECTORY))
            strcat(args, "\\");
    }
    if(strlen(args) && args[strlen(args) - 1] == '\\') strcat(args, "*.*");
    else if(!strcmp(args, ".") || (strlen(args) > 1 && (args[strlen(args) - 2] == ':' || args[strlen(args) - 2] == '\\') && args[strlen(args) - 1] == '.')) {
        args[strlen(args) - 1] = '*';
        strcat(args, ".*");
    }
    else if(uselfn && strchr(args, '*')) {
        char* find_last;
        find_last = strrchr_dbcs(args, '\\');
        if(find_last == NULL) find_last = args;
        else find_last++;
        if(strlen(find_last) > 0 && args[strlen(args) - 1] == '*' && strchr(find_last, '.') == NULL) strcat(args, ".*");
    }
    if(!strcmp(args, "*.*") || (strlen(args) > 3 && (!strcmp(args + strlen(args) - 4, "\\*.*") || !strcmp(args + strlen(args) - 4, ":*.*")))) {
        if(!optQ) {
        first_1:
            WriteOut(MSG_Get("SHELL_CMD_DEL_SURE"));
        first_2:
            uint8_t c; uint16_t n = 1;
            DOS_ReadFile(STDIN, &c, &n);
            do {
                if(c == ch_n || c == ch_N) {
                    DOS_WriteFile(STDOUT, &c, &n);
                    DOS_ReadFile(STDIN, &c, &n);
                    do switch(c) {
                    case 0xD: WriteOut("\n"); dos.dta(save_dta); return;
                    case 0x03: dos.dta(save_dta); return;
                    case 0x08: WriteOut("\b \b"); goto first_2;
                    } while(DOS_ReadFile(STDIN, &c, &n));
                }

                if(c == ch_y || c == ch_Y) {
                    DOS_WriteFile(STDOUT, &c, &n);
                    DOS_ReadFile(STDIN, &c, &n);
                    do switch(c) {
                    case 0xD: WriteOut("\n"); goto continue_1;
                    case 0x03: dos.dta(save_dta); return;
                    case 0x08: WriteOut("\b \b"); goto first_2;
                    } while(DOS_ReadFile(STDIN, &c, &n));
                }

                if(c == 0xD) { WriteOut("\n"); goto first_1; }
                if(c == 0x03) { dos.dta(save_dta); return; }
                if(c == '\t' || c == 0x08) goto first_2;

                DOS_WriteFile(STDOUT, &c, &n);
                DOS_ReadFile(STDIN, &c, &n);
                do switch(c) {
                case 0xD: WriteOut("\n"); goto first_1;
                case 0x03: dos.dta(save_dta); return;
                case 0x08: WriteOut("\b \b"); goto first_2;
                } while(DOS_ReadFile(STDIN, &c, &n));
                goto first_2;
            } while(DOS_ReadFile(STDIN, &c, &n));
        }
    }

continue_1:
    /* Command uses dta so set it to our internal dta */
    if(!DOS_Canonicalize(args, full)) { WriteOut(MSG_Get("SHELL_ILLEGAL_PATH")); dos.dta(save_dta); return; }
    char path[DOS_PATHLENGTH], spath[DOS_PATHLENGTH], pattern[DOS_PATHLENGTH], * r = strrchr_dbcs(full, '\\');
    if(r != NULL) {
        *r = 0;
        strcpy(path, full);
        strcat(path, "\\");
        strcpy(pattern, r + 1);
        *r = '\\';
    }
    else {
        strcpy(path, "");
        strcpy(pattern, full);
    }
    int k = 0;
    for(int i = 0; i < (int)strlen(pattern); i++)
        if(pattern[i] != '\"')
            pattern[k++] = pattern[i];
    pattern[k] = 0;
    strcpy(spath, path);
    if(strchr(args, '\"') || uselfn) {
        if(!DOS_GetSFNPath(("\"" + std::string(path) + "\\").c_str(), spath, false)) strcpy(spath, path);
        if(!strlen(spath) || spath[strlen(spath) - 1] != '\\') strcat(spath, "\\");
    }
    std::string pfull = std::string(spath) + std::string(pattern);
    int fbak = lfn_filefind_handle;
    lfn_filefind_handle = uselfn ? LFN_FILEFIND_INTERNAL : LFN_FILEFIND_NONE;
    bool res = DOS_FindFirst(((uselfn && pfull.length() && pfull[0] != '"' ? "\"" : "") + pfull + (uselfn && pfull.length() && pfull[pfull.length() - 1] != '"' ? "\"" : "")).c_str(), 0xffff & ~DOS_ATTR_VOLUME);
    if(!res) {
        lfn_filefind_handle = fbak;
        WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"), args);
        dos.dta(save_dta);
        return;
    }
    lfn_filefind_handle = fbak;
    //end can't be 0, but if it is we'll get a nice crash, who cares :)
    strcpy(sfull, full);
    char* end = strrchr_dbcs(full, '\\') + 1; *end = 0;
    char* lend = strrchr_dbcs(sfull, '\\') + 1; *lend = 0;
    dta = dos.dta();
    bool exist = false;
    lfn_filefind_handle = uselfn ? LFN_FILEFIND_INTERNAL : LFN_FILEFIND_NONE;
    while(res) {
        dta.GetResult(name, lname, size, hsize, date, time, attr);
        if(!optF && (attr & DOS_ATTR_READ_ONLY) && !(attr & DOS_ATTR_DIRECTORY)) {
            exist = true;
            strcpy(end, name);
            strcpy(lend, lname);
            WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"), uselfn ? sfull : full);
        }
        else if(!(attr & DOS_ATTR_DIRECTORY)) {
            exist = true;
            strcpy(end, name);
            strcpy(lend, lname);
            if(optP) {
                WriteOut(MSG_Get("SHELL_CMD_DEL_CONFIRM"), uselfn ? sfull : full);
                uint8_t c;
                uint16_t n = 1;
                DOS_ReadFile(STDIN, &c, &n);
                if(c == 3) break;
                c = c == ch_y || c == ch_Y ? ch_Y : ch_N;
                WriteOut("%c\r\n", c);
                if(c == ch_N) { lfn_filefind_handle = uselfn ? LFN_FILEFIND_INTERNAL : LFN_FILEFIND_NONE; res = DOS_FindNext(); continue; }
            }
            if(strlen(full)) {
                std::string pfull = (uselfn || strchr(full, ' ') ? (full[0] != '"' ? "\"" : "") : "") + std::string(full) + (uselfn || strchr(full, ' ') ? (full[strlen(full) - 1] != '"' ? "\"" : "") : "");
                bool reset = false;
                if(optF && (attr & DOS_ATTR_READ_ONLY) && DOS_SetFileAttr(pfull.c_str(), attr & ~DOS_ATTR_READ_ONLY)) reset = true;
                if(!DOS_UnlinkFile(pfull.c_str())) {
                    if(optF && reset) DOS_SetFileAttr(pfull.c_str(), attr);
                    WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"), uselfn ? sfull : full);
                }
            }
            else WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"), uselfn ? sfull : full);
        }
        res = DOS_FindNext();
    }
    lfn_filefind_handle = fbak;
    if(!exist) WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"), args);
    dos.dta(save_dta);
}

size_t GetPauseCount() {
	uint16_t rows;
	if (IS_PC98_ARCH)
		rows=real_readb(0x60,0x113) & 0x01 ? 25 : 20;
	else
		rows=(IS_EGAVGA_ARCH?real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS):24)+1;
	return (rows > 2u) ? (rows - 2u) : 23u;
}

struct DtaResult {
	char name[DOS_NAMELENGTH_ASCII];
	char lname[LFN_NAMELENGTH+1];
	uint32_t size;
	uint32_t hsize;
	uint16_t date;
	uint16_t time;
	uint8_t attr;

	static bool groupDef(const DtaResult &lhs, const DtaResult &rhs) { return (lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY)?true:((((lhs.attr & DOS_ATTR_DIRECTORY) && (rhs.attr & DOS_ATTR_DIRECTORY)) || (!(lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY))) && strcmp(lhs.name, rhs.name) < 0); }
	static bool groupExt(const DtaResult &lhs, const DtaResult &rhs) { return (lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY)?true:((((lhs.attr & DOS_ATTR_DIRECTORY) && (rhs.attr & DOS_ATTR_DIRECTORY)) || (!(lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY))) && strcmp(lhs.getExtension(), rhs.getExtension()) < 0); }
	static bool groupSize(const DtaResult &lhs, const DtaResult &rhs) { return (lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY)?true:((((lhs.attr & DOS_ATTR_DIRECTORY) && (rhs.attr & DOS_ATTR_DIRECTORY)) || (!(lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY))) && lhs.size+lhs.hsize*0x100000000 < rhs.size+rhs.hsize*0x100000000); }
	static bool groupDate(const DtaResult &lhs, const DtaResult &rhs) { return (lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY)?true:((((lhs.attr & DOS_ATTR_DIRECTORY) && (rhs.attr & DOS_ATTR_DIRECTORY)) || (!(lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY))) && (lhs.date < rhs.date || (lhs.date == rhs.date && lhs.time < rhs.time))); }
	static bool groupRevDef(const DtaResult &lhs, const DtaResult &rhs) { return (lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY)?true:((((lhs.attr & DOS_ATTR_DIRECTORY) && (rhs.attr & DOS_ATTR_DIRECTORY)) || (!(lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY))) && strcmp(lhs.name, rhs.name) > 0); }
	static bool groupRevExt(const DtaResult &lhs, const DtaResult &rhs) { return (lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY)?true:((((lhs.attr & DOS_ATTR_DIRECTORY) && (rhs.attr & DOS_ATTR_DIRECTORY)) || (!(lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY))) && strcmp(lhs.getExtension(), rhs.getExtension()) > 0); }
	static bool groupRevSize(const DtaResult &lhs, const DtaResult &rhs) { return (lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY)?true:((((lhs.attr & DOS_ATTR_DIRECTORY) && (rhs.attr & DOS_ATTR_DIRECTORY)) || (!(lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY))) && lhs.size+lhs.hsize*0x100000000 > rhs.size+rhs.hsize*0x100000000); }
	static bool groupRevDate(const DtaResult &lhs, const DtaResult &rhs) { return (lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY)?true:((((lhs.attr & DOS_ATTR_DIRECTORY) && (rhs.attr & DOS_ATTR_DIRECTORY)) || (!(lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY))) && (lhs.date > rhs.date || (lhs.date == rhs.date && lhs.time > rhs.time))); }
	static bool groupDirs(const DtaResult &lhs, const DtaResult &rhs) { return (lhs.attr & DOS_ATTR_DIRECTORY) && !(rhs.attr & DOS_ATTR_DIRECTORY); }
	static bool compareName(const DtaResult &lhs, const DtaResult &rhs) { return strcmp(lhs.name, rhs.name) < 0; }
	static bool compareExt(const DtaResult &lhs, const DtaResult &rhs) { return strcmp(lhs.getExtension(), rhs.getExtension()) < 0; }
	static bool compareSize(const DtaResult &lhs, const DtaResult &rhs) { return lhs.size+lhs.hsize*0x100000000 < rhs.size+rhs.hsize*0x100000000; }
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

extern bool ctrlbrk;
std::vector<std::string> tdirs;

static bool doDeltree(DOS_Shell * shell, char * args, DOS_DTA dta, bool optY, bool first) {
    const char ch_y = MSG_Get("INT21_6523_YESNO_CHARS")[0];
    const char ch_n = MSG_Get("INT21_6523_YESNO_CHARS")[1];
    const char ch_Y = toupper(ch_y);
    const char ch_N = toupper(ch_n);
    char spath[DOS_PATHLENGTH],sargs[DOS_PATHLENGTH+4],path[DOS_PATHLENGTH+4],full[DOS_PATHLENGTH],sfull[DOS_PATHLENGTH+2];
	if (!DOS_Canonicalize(args,full)||strrchr_dbcs(full,'\\')==NULL) { shell->WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));return false; }
	if (!DOS_GetSFNPath(args,spath,false)) {
		if (first) shell->WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
		return false;
	}
	if (!uselfn||!DOS_GetSFNPath(args,sfull,true)) strcpy(sfull,full);
    sprintf(sargs,"\"%s\"",spath);
    bool found=false, fdir=false, res=DOS_FindFirst(sargs,0xffff & ~DOS_ATTR_VOLUME);
	if (!res) return false;
	//end can't be 0, but if it is we'll get a nice crash, who cares :)
    uint16_t attribute=0;
	strcpy(path,full);
    if (!first&&strlen(args)>3&&!strcmp(args+strlen(args)-4,"\\.\\.")) {
        if (strlen(path)&&path[strlen(path)-1]=='\\') path[strlen(path)-1]=0;
        if (strlen(path)&&path[strlen(path)-1]!=':') {
            bool reset=false;
            if(DOS_GetFileAttr(path,&attribute) && (attribute&DOS_ATTR_READ_ONLY)&&DOS_SetFileAttr(path, attribute & ~DOS_ATTR_READ_ONLY)) reset=true;
            if (!DOS_RemoveDir(path)&&!(uselfn&&DOS_RemoveDir(sfull))) {
                if (reset) DOS_SetFileAttr(path, attribute);
                shell->WriteOut(MSG_Get("SHELL_CMD_RMDIR_ERROR"),uselfn?sfull:full);
            }
        }
        return true;
    }
	*(strrchr_dbcs(path,'\\')+1)=0;
	char * end=strrchr_dbcs(full,'\\')+1;*end=0;
	char * lend=strrchr_dbcs(sfull,'\\')+1;*lend=0;
    char name[DOS_NAMELENGTH_ASCII],lname[LFN_NAMELENGTH+1];
    uint32_t size,hsize;uint16_t time,date;uint8_t attr;uint16_t fattr;
    std::vector<std::string> cdirs, cfiles;
    cdirs.clear();
	cfiles.clear();
    std::string pfull;
	while (res) {
        strcpy(spath, path);
		dta.GetResult(name,lname,size,hsize,date,time,attr);
		if (!((!strcmp(name, ".") || !strcmp(name, "..")) && attr & DOS_ATTR_DIRECTORY)) {
			found=true;
			strcpy(end,name);
			strcpy(lend,lname);
			if (strlen(full)&&DOS_GetFileAttr(((uselfn||strchr(full, ' ')?(full[0]!='"'?"\"":""):"")+std::string(full)+(uselfn||strchr(full, ' ')?(full[strlen(full)-1]!='"'?"\"":""):"")).c_str(), &fattr)) {
                uint8_t c;
                uint16_t n=1;
                if(attr&DOS_ATTR_DIRECTORY) {
                    if (strcmp(name, ".")&&strcmp(name, "..")) {
                        if (!optY&&first) {
                            shell->WriteOut(MSG_Get("SHELL_CMD_RMDIR_FULLTREE_CONFIRM"), uselfn ? sfull : full);
                            DOS_ReadFile (STDIN,&c,&n);
                            if (c==3) {shell->WriteOut("^C\r\n");break;}
                            c = c==ch_y||c==ch_Y ? ch_Y:ch_N;
                            shell->WriteOut("%c\r\n", c);
                            if (c==ch_N) {res = DOS_FindNext();continue;}
                        }
                        fdir=true;
                        strcat(spath, name);
                        strcat(spath, "\\*.*");
                        cdirs.emplace_back(std::string(spath));
                    }
                } else {
                    if (!optY&&first) {
                        shell->WriteOut(MSG_Get("SHELL_CMD_RMDIR_SINGLE_CONFIRM"), uselfn ? sfull : full);
                        DOS_ReadFile (STDIN,&c,&n);
                        if (c==3) {shell->WriteOut("^C\r\n");break;}
                        c = c==ch_y||c==ch_Y ? ch_Y:ch_N;
                        shell->WriteOut("%c\r\n", c);
                        if (c==ch_N) {res = DOS_FindNext();continue;}
                    }
                    pfull=(uselfn||strchr(uselfn?sfull:full, ' ')?((uselfn?sfull:full)[0]!='"'?"\"":""):"")+std::string(uselfn?sfull:full)+(uselfn||strchr(uselfn?sfull:full, ' ')?((uselfn?sfull:full)[strlen(uselfn?sfull:full)-1]!='"'?"\"":""):"");
                    cfiles.push_back(pfull);
                }
            }
		}
		res=DOS_FindNext();
	}
    while (!cfiles.empty()) {
        bool reset=false;
        pfull = std::string(cfiles.begin()->c_str());
        if ((attr & DOS_ATTR_READ_ONLY)&&DOS_SetFileAttr(pfull.c_str(), attr & ~DOS_ATTR_READ_ONLY)) reset=true;
        if (!DOS_UnlinkFile(pfull.c_str())) {
            if (reset) DOS_SetFileAttr(pfull.c_str(), attr);
            shell->WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"),pfull.c_str());
        }
        cfiles.erase(cfiles.begin());
    }
    if (!first&&strlen(args)>4&&!strcmp(args+strlen(args)-4,"\\*.*")) {
        end=strrchr_dbcs(full,'\\')+1;*end=0;
        lend=strrchr_dbcs(sfull,'\\')+1;*lend=0;
        if (fdir) {
            strcpy(spath, path);
            strcat(spath, ".\\.");
            cdirs.push_back(std::string(spath));
        } else {
            if (strlen(path)&&path[strlen(path)-1]=='\\') path[strlen(path)-1]=0;
            if (strlen(path)&&path[strlen(path)-1]!=':') {
                bool reset=false;
                if(DOS_GetFileAttr(path,&attribute) && (attribute&DOS_ATTR_READ_ONLY)&&DOS_SetFileAttr(path, attribute & ~DOS_ATTR_READ_ONLY)) reset=true;
                if (!DOS_RemoveDir(path)&&!(uselfn&&DOS_RemoveDir(sfull))) {
                    if (reset) DOS_SetFileAttr(path, attribute);
                    shell->WriteOut(MSG_Get("SHELL_CMD_RMDIR_ERROR"),uselfn?sfull:full);
                }
            }
        }
    }
    tdirs.insert(tdirs.begin()+1, cdirs.begin(), cdirs.end());
	return found;
}

void DOS_Shell::CMD_DELTREE(char * args) {
	//HELP("DELTREE");
	StripSpaces(args);
	bool optY=ScanCMDBool(args,"Y");
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
    if (!*args) {
		WriteOut(MSG_Get("SHELL_MISSING_PARAMETER"));
		return;
	}

	if (uselfn&&strchr(args, '*')) {
		char * find_last;
		find_last=strrchr_dbcs(args,'\\');
		if (find_last==NULL) find_last=args;
		else find_last++;
		if (strlen(find_last)>0&&args[strlen(args)-1]=='*'&&strchr(find_last, '.')==NULL) strcat(args, ".*");
	}
	char buffer[CROSS_LEN];
	args = ExpandDot(args,buffer, CROSS_LEN, true);
	StripSpaces(args);
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	tdirs.clear();
	tdirs.emplace_back(std::string(args));
	bool first=true, found=false;
    ctrlbrk=false;
    inshell=true;
	while (!tdirs.empty()) {
		if (doDeltree(this, (char *)tdirs.begin()->c_str(), dta, optY, first))
			found=true;
        first=false;
		tdirs.erase(tdirs.begin());
	}
    inshell=true;
	if (!found) WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
	dos.dta(save_dta);
}

bool CheckBreak(DOS_Shell * shell) {
    if (ctrlbrk || dos.errorcode == 77) {
        if (dos.errorcode == 77) dos.errorcode = 0;
        else if (ctrlbrk) {
            ctrlbrk=false;
            uint8_t c;uint16_t n=1;
            DOS_ReadFile (STDIN,&c,&n);
            if (c == 3 && (inshell || dos.errorcode == 77)) shell->WriteOut("^C\r\n");
            if (dos.errorcode == 77) dos.errorcode = 0;
            ctrlbrk=false;
        }
        return true;
    } else
        return false;
}

bool cont[200];
static bool doTree(DOS_Shell * shell, char * args, DOS_DTA dta, bool optA, bool optF) {
    char *p=strchr(args, ':');
    bool found=false, last=false, plast=false;
    int level=1;
    if (p) {
        *p=0;
        if (*args=='-') {
            plast=true;
            args++;
        }
        level=atoi(args);
        args=p+1;
        if (tdirs.size()<2) last=true;
        else {
            char * arg=(char *)(tdirs.begin()+1)->c_str();
            p=strchr(arg, ':');
            if (p) {
                *p=0;
                if (level!=atoi(*arg=='-'?arg+1:arg)) last=true;
                *p=':';
            }
        }
    }
    if (level>=200) return false;
    char spath[DOS_PATHLENGTH],sargs[DOS_PATHLENGTH+4],path[DOS_PATHLENGTH+4],full[DOS_PATHLENGTH],sfull[DOS_PATHLENGTH+2];
	if (!DOS_Canonicalize(args,full)||strrchr_dbcs(full,'\\')==NULL) { shell->WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));return level; }
	if (!DOS_GetSFNPath(args,spath,false)) {
		if (!level) shell->WriteOut(MSG_Get("SHELL_CMD_TREE_ERROR"));
		return level;
	}
	if (!uselfn||!DOS_GetSFNPath(args,sfull,true)) strcpy(sfull,full);
    if (level&&strlen(sfull)>4&&!strcasecmp(sfull+strlen(sfull)-4, "\\*.*")) {
        *(sfull+strlen(sfull)-4)=0;
        p=strrchr_dbcs(sfull, '\\');
        char c=optA?(last?'\\':'+'):(last?0xc0:0xc3);
        cont[level]=!last;
        for (int i=1; i<level; i++) shell->WriteOut("%c   ", cont[i]?(optA?'|':0xb3):' ');
        shell->WriteOut(("%c"+std::string(3, optA?'-':0xc4)+"%s\n").c_str(), c, p?p+1:sfull);
        *(sfull+strlen(sfull))='\\';
    }
    sprintf(sargs,"\"%s\"",spath);
    bool res=DOS_FindFirst(sargs,0xffff & ~DOS_ATTR_VOLUME);
    if (!res) {
        if (!level) shell->WriteOut(MSG_Get("SHELL_CMD_TREE_ERROR"));
        return level;
    }
    //uint16_t attribute=0; UNUSED
	strcpy(path,full);
	*(strrchr_dbcs(path,'\\')+1)=0;
	char * end=strrchr_dbcs(full,'\\')+1;*end=0;
	char * lend=strrchr_dbcs(sfull,'\\')+1;*lend=0;
    char name[DOS_NAMELENGTH_ASCII],lname[LFN_NAMELENGTH+1];
    uint32_t size,hsize;uint16_t time,date;uint8_t attr;uint16_t fattr;
    std::vector<std::string> cdirs;
    cdirs.clear();
	while (res) {
        if (CheckBreak(shell)) return false;
        strcpy(spath,((plast||(level==1&&last)?"-":"")+std::to_string(level+1)+":").c_str());
        strcat(spath, path);
		dta.GetResult(name,lname,size,hsize,date,time,attr);
		if (!((!strcmp(name, ".") || !strcmp(name, "..")) && attr & DOS_ATTR_DIRECTORY)) {
			strcpy(end,name);
			strcpy(lend,lname);
			if (strlen(full)&&DOS_GetFileAttr(((uselfn||strchr(full, ' ')?(full[0]!='"'?"\"":""):"")+std::string(full)+(uselfn||strchr(full, ' ')?(full[strlen(full)-1]!='"'?"\"":""):"")).c_str(), &fattr)) {
                if(attr&DOS_ATTR_DIRECTORY) {
                    if (strcmp(name, ".")&&strcmp(name, "..")) {
                        strcat(spath, name);
                        strcat(spath, "\\*.*");
                        cdirs.emplace_back(std::string(spath));
                        found=true;
                    }
                } else if (optF) {
                    for (int i=1; i<=level; i++) shell->WriteOut("%c   ", (i==1&&level>1?!plast:cont[i])?(optA?'|':0xb3):' ');
                    shell->WriteOut("    %s\n", uselfn?lname:name);
                }
            }
		}
		res=DOS_FindNext();
	}
    if (!found&&!level) {
        shell->WriteOut(MSG_Get("SHELL_CMD_TREE_ERROR"));
        return false;
    }
    tdirs.insert(tdirs.begin()+1, cdirs.begin(), cdirs.end());
	return true;
}

bool tree=false;
void DOS_Shell::CMD_TREE(char * args) {
	//HELP("TREE");
	StripSpaces(args);
	bool optA=ScanCMDBool(args,"A");
	bool optF=ScanCMDBool(args,"F");
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	StripSpaces(args);
	char buffer[CROSS_LEN];
    strcpy(buffer, "0:");
    strcat(buffer, *args?args:".");
    if (strlen(args)==2&&args[1]==':') strcat(buffer, ".");
    if (args[strlen(args)-1]!='\\') strcat(buffer, "\\");
    strcat(buffer, "*.*");
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
    if (strlen(args)>1&&args[1]==':') {
        char c[]=" _:";
        c[1]=toupper(args[0]);
        if (!Drives[c[1]-'A']) {
            WriteOut(MSG_Get("SHELL_ILLEGAL_DRIVE"));
            return;
        }
        tree=true;
        CMD_VOL(c[1]>='A'&&c[1]<='Z'?c:empty_string);
        tree=false;
        WriteOut("%c:%s\n", c[1], *args?args+2:".");
    } else {
        tree=true;
        CMD_VOL(empty_string);
        tree=false;
        uint8_t drive=DOS_GetDefaultDrive();
        WriteOut("%c:%s\n", 'A'+drive, *args?args:".");
    }
    for (int i=0; i<200; i++) cont[i]=false;
    ctrlbrk=false;
    inshell=true;
	tdirs.clear();
	tdirs.emplace_back(std::string(buffer));
	while (!tdirs.empty()) {
		if (!doTree(this, (char *)tdirs.begin()->c_str(), dta, optA, optF)) break;
		tdirs.erase(tdirs.begin());
	}
    inshell=false;
	dos.dta(save_dta);
}

void DOS_Shell::CMD_HELP(char * args){
	HELP("HELP");
	bool optall=ScanCMDBool(args,"A")|ScanCMDBool(args,"ALL");
	/* Print the help */
	args = trim(args);
	upcase(args);
	uint8_t attr = DOS_GetAnsiAttr();
	if(!optall&&!*args) WriteOut(MSG_Get("SHELL_CMD_HELP"));
	uint32_t cmd_index=0,write_count=0;
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
					uint8_t c;uint16_t n=1;
					DOS_ReadFile(STDIN,&c,&n);
					if (c==3) {if (attr) DOS_SetAnsiAttr(attr);return;}
					if (c==0) DOS_ReadFile(STDIN,&c,&n); // read extended key
				}
			}
		}
		cmd_index++;
	}
	if (optall&&show)
		WriteOut(MSG_Get("SHELL_CMD_HELP_END1"));
	else if (*args&&!show) {
		std::string argc=std::string(StripArg(args));
		if (argc!=""&&argc!="CWSDPMI") DoCommand((char *)(argc+(argc=="DOS4GW"||argc=="DOS32A"||argc=="ZIP"||argc=="UNZIP"?"":" /?")).c_str());
	}
	if (!*args&&show)
		WriteOut(MSG_Get("SHELL_CMD_HELP_END2"));
	if (attr) DOS_SetAnsiAttr(attr);
}

void removeChar(char *str, char c) {
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
	char* slash = strrchr_dbcs(arg1,'\\');
	uint32_t size,hsize;uint16_t date;uint16_t time;uint8_t attr;
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
		char* dummy = strrchr_dbcs(dir_source,'\\');
		if (!dummy) dummy = strrchr(dir_source,':');
		if (!dummy) { //Possible due to length
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			return;
		}
		dummy++;
		*dummy = 0;
		if (strchr_dbcs(arg2,'\\')||strchr(arg2,':')) {
			safe_strncpy(dir_target,arg2,DOS_PATHLENGTH + 4);
			dummy = strrchr_dbcs(dir_target,'\\');
			if (!dummy) dummy = strrchr(dir_target,':');
			if (dummy) {
				dummy++;
				*dummy = 0;
				if (strcasecmp(dir_source, dir_target)) {
					WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
					return;
				}
			}
			arg2=strrchr_dbcs(arg2,strrchr_dbcs(arg2,'\\')?'\\':':')+1;
		}
		if (strlen(dummy)&&dummy[strlen(dummy)-1]==':')
			strcat(dummy, ".\\");
	} else {
		if (strchr_dbcs(arg2,'\\')||strchr(arg2,':')) {SyntaxError();return;};
		strcpy(dir_source, ".\\");
	}
	
	strcpy(target,arg2);

	char path[DOS_PATHLENGTH], spath[DOS_PATHLENGTH], pattern[DOS_PATHLENGTH], full[DOS_PATHLENGTH], *r;
	if (!DOS_Canonicalize(arg1,full)) return;
	r=strrchr_dbcs(full, '\\');
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
	if (!DOS_FindFirst(((uselfn&&pfull.length()&&pfull[0]!='"'?"\"":"")+pfull+(uselfn&&pfull.length()&&pfull[pfull.length()-1]!='"'?"\"":"")).c_str(), strchr(arg1,'*')!=NULL || strchr(arg1,'?')!=NULL ? 0xffff & ~DOS_ATTR_VOLUME & ~DOS_ATTR_DIRECTORY : 0xffff & ~DOS_ATTR_VOLUME)) {
		lfn_filefind_handle=fbak;
		WriteOut(MSG_Get("SHELL_CMD_RENAME_ERROR"),arg1);
	} else {
		std::vector<std::string> sources;
		sources.clear();
	
		do {    /* File name and extension */
			dta.GetResult(name,lname,size,hsize,date,time,attr);
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
				sources.emplace_back(uselfn?((sargs[0]!='"'?"\"":"")+std::string(sargs)+(sargs[strlen(sargs)-1]!='"'?"\"":"")).c_str():sargs);
				sources.emplace_back(uselfn?((targs[0]!='"'?"\"":"")+std::string(targs)+(targs[strlen(targs)-1]!='"'?"\"":"")).c_str():targs);
				sources.emplace_back(strlen(sargs)>2&&sargs[0]=='.'&&sargs[1]=='\\'?sargs+2:sargs);
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
		WriteOut_NoParsing(args, true);
		WriteOut("\n");
	} else {
		WriteOut_NoParsing(args, true);
		WriteOut("\r\n");
	}
}

void DOS_Shell::CMD_EXIT(char * args) {
	HELP("EXIT");
	exit = true;
}

std::vector<uint8_t> olddrives;
std::vector<std::string> olddirs;
void DOS_Shell::CMD_PUSHD(char * args) {
	HELP("PUSHD");
	StripSpaces(args);
	char sargs[CROSS_LEN];
	if (strlen(args)>1 && args[1]==':' && toupper(args[0])>='A' && toupper(args[0])<='Z' && !Drives[toupper(args[0])-'A']) {
        WriteOut(MSG_Get("SHELL_ILLEGAL_DRIVE"));
        return;
    }
	if (*args && !DOS_GetSFNPath(args,sargs,false)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}
	if (*args) {
        char dir[DOS_PATHLENGTH];
        uint8_t drive = DOS_GetDefaultDrive()+'A';
        DOS_GetCurrentDir(0,dir,true);
        if (strlen(args)>1 && args[1]==':') DOS_SetDefaultDrive(toupper(args[0])-'A');
        if (DOS_ChangeDir(sargs)) {
            olddrives.push_back(drive);
            olddirs.emplace_back(std::string(dir));
        } else {
            if (strlen(args)>1 && args[1]==':') DOS_SetDefaultDrive(drive-'A');
            WriteOut(MSG_Get("SHELL_CMD_CHDIR_ERROR"),args);
        }
    } else {
        for (int i=(int)(olddrives.size()-1); i>=0; i--)
            if (olddrives.at(i)>='A'&&olddrives.at(i)<='Z')
                WriteOut("%c:\\%s\n",olddrives.at(i),olddirs.at(i).c_str());
    }
}

void DOS_Shell::CMD_POPD(char * args) {
	HELP("POPD");
    if (!olddrives.size()) return;
    uint8_t olddrive=olddrives.back();
    std::string olddir=olddirs.back();
    if (olddrive>='A'&&olddrive<='Z'&&Drives[olddrive-'A']) {
        uint8_t drive = DOS_GetDefaultDrive()+'A';
        if (olddrive!=DOS_GetDefaultDrive()+'A') DOS_SetDefaultDrive(olddrive-'A');
        if (Drives[DOS_GetDefaultDrive()]->TestDir(olddir.c_str()))
            strcpy(Drives[DOS_GetDefaultDrive()]->curdir,olddir.c_str());
        else
            DOS_SetDefaultDrive(drive-'A');
    }
    olddrives.pop_back();
    olddirs.pop_back();
}

void DOS_Shell::CMD_CHDIR(char * args) {
	HELP("CHDIR");
	StripSpaces(args);
	char sargs[CROSS_LEN];
	if (*args && !DOS_GetSFNPath(args,sargs,false)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}
	uint8_t drive = DOS_GetDefaultDrive()+'A';
	char dir[DOS_PATHLENGTH];
	if (!*args) {
		DOS_GetCurrentDir(0,dir,true);
		WriteOut("%c:\\",drive);
		WriteOut_NoParsing(dir, true);
		WriteOut("\n");
	} else if(strlen(args) == 2 && args[1]==':') {
		uint8_t targetdrive = (args[0] | 0x20)-'a' + 1;
		unsigned char targetdisplay = *reinterpret_cast<unsigned char*>(&args[0]);
        if(!DOS_GetCurrentDir(targetdrive,dir,true)) { // verify that this should be true
			if(drive == 'Z') {
				WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),toupper(targetdisplay));
			} else {
				WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			}
			return;
		}
		WriteOut("%c:\\",toupper(targetdisplay));
		WriteOut_NoParsing(dir, true);
		WriteOut("\n");
		if(drive == 'Z')
			WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT"),toupper(targetdisplay));
	} else if (!DOS_ChangeDir(sargs)) {
		/* Changedir failed. Check if the filename is longer than 8 and/or contains spaces */
	   
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
		WriteOut(MSG_Get(dos.errorcode==DOSERR_ACCESS_DENIED?"SHELL_CMD_MKDIR_EXIST":"SHELL_CMD_MKDIR_ERROR"),args);
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

static void FormatNumber(uint64_t num,char * buf) {
	uint64_t numo = num;
	uint32_t numb,numk,numm,numg,nummi,numgi,numti,numpi,numei;
	numb=num % 1000;
	num/=1000;
	numk=num % 1000;
	num/=1000;
	numm=num % 1000;
	num/=1000;
	numg=num % 1000;
	numo/=1024;
	numo/=1024;
	nummi=(numo % 1024) / 10.24 + 0.5;
	numo/=1024;
	numgi=numo % 1000;
	numo/=1000;
	numti=numo % 1000;
	numo/=1000;
	numpi=numo % 1000;
	numei=numo / 1000;
	if (numei) {
		sprintf(buf,"%u%c%03u%c%03u%c%03u%c%02u G",numei,dos.tables.country[7],numpi,dos.tables.country[7],numti,dos.tables.country[7],numgi,dos.tables.country[9],nummi);
		return;
	}
	if (numpi) {
		sprintf(buf,"%u%c%03u%c%03u%c%02u G",numpi,dos.tables.country[7],numti,dos.tables.country[7],numgi,dos.tables.country[9],nummi);
		return;
	}
	if (numti) {
		sprintf(buf,"%u%c%03u%c%02u G",numti,dos.tables.country[7],numgi,dos.tables.country[9],nummi);
		return;
	}
	if (numg) {
		if (numgi>127) sprintf(buf,"%u%c%02u G",numgi,dos.tables.country[9],nummi);
		else sprintf(buf,"%u%c%03u%c%03u%c%03u",numg,dos.tables.country[7],numm,dos.tables.country[7],numk,dos.tables.country[7],numb);
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
char *FormatDate(uint16_t year, uint8_t month, uint8_t day) {
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

uint64_t byte_count;
uint32_t file_count,dir_count;
Bitu p_count;
std::vector<std::string> dirs, adirs;
static bool dirPaused(DOS_Shell * shell, Bitu w_size, bool optP, bool optW, bool show=true) {
	p_count+=optW?5:1;
	if (optP && p_count%(GetPauseCount()*w_size)<1) {
		shell->WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
		uint8_t c;uint16_t n=1;
		DOS_ReadFile(STDIN,&c,&n);
		if (c==3) {if (show) shell->WriteOut("^C\r\n");return false;}
		if (c==0) DOS_ReadFile(STDIN,&c,&n); // read extended key
	}
	return true;
}

static bool doDir(DOS_Shell * shell, char * args, DOS_DTA dta, char * numformat, Bitu w_size, bool optW, bool optZ, bool optS, bool optP, bool optB, bool optA, bool optAD, bool optAminusD, bool optAS, bool optAminusS, bool optAH, bool optAminusH, bool optAR, bool optAminusR, bool optAA, bool optAminusA, bool optOGN, bool optOGD, bool optOGE, bool optOGS, bool optOG, bool optON, bool optOD, bool optOE, bool optOS, bool reverseSort, bool rev2Sort) {
	char path[DOS_PATHLENGTH];
	char sargs[CROSS_LEN], largs[CROSS_LEN], buffer[CROSS_LEN];
    unsigned int tcols=IS_PC98_ARCH?80:real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
    if (!tcols) tcols=80;

	/* Make a full path in the args */
	if (!DOS_Canonicalize(args,path)) {
		shell->WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return true;
	}
	*(strrchr_dbcs(path,'\\')+1)=0;
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
				uint8_t c;uint16_t n=1;
				DOS_ReadFile(STDIN,&c,&n);
				if (c==3) return false;
				if (c==0) DOS_ReadFile(STDIN,&c,&n); // read extended key
			}
		}
	}
    if (*(sargs+strlen(sargs)-1) != '\\') strcat(sargs,"\\");

	uint64_t cbyte_count=0;
	uint32_t cfile_count=0,w_count=0;
	int fbak=lfn_filefind_handle;
	lfn_filefind_handle=uselfn&&!optZ?LFN_FILEFIND_INTERNAL:LFN_FILEFIND_NONE;
	bool ret=DOS_FindFirst(args,0xffff & ~DOS_ATTR_VOLUME), found=true, first=true;
	if (!ret) {
		size_t len = strlen(args);
		args = ExpandDotMore(args,buffer,CROSS_LEN);
		if (strlen(args)!=len) ret=DOS_FindFirst(args,0xffff & ~DOS_ATTR_VOLUME);
	}
	lfn_filefind_handle=fbak;
	if (ret) {
		std::vector<DtaResult> results;

		lfn_filefind_handle=uselfn&&!optZ?LFN_FILEFIND_INTERNAL:LFN_FILEFIND_NONE;
		do {    /* File name and extension */
			DtaResult result;
			dta.GetResult(result.name,result.lname,result.size,result.hsize,result.date,result.time,result.attr);

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

		bool oneRev = (reverseSort||rev2Sort)&&reverseSort!=rev2Sort;
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
		} else if (optOGN) {
			// Directories first, then files, both sort by name
			std::sort(results.begin(), results.end(), oneRev?DtaResult::groupRevDef:DtaResult::groupDef);
		} else if (optOGE) {
			// Directories first, then files, both sort by extension
			std::sort(results.begin(), results.end(), oneRev?DtaResult::groupRevExt:DtaResult::groupExt);
		} else if (optOGS) {
			// Directories first, then files, both sort by size
			std::sort(results.begin(), results.end(), oneRev?DtaResult::groupRevSize:DtaResult::groupSize);
		} else if (optOGD) {
			// Directories first, then files, both sort by date
			std::sort(results.begin(), results.end(), oneRev?DtaResult::groupRevDate:DtaResult::groupDate);
		}
		if (reverseSort) std::reverse(results.begin(), results.end());

		for (std::vector<DtaResult>::iterator iter = results.begin(); iter != results.end(); ++iter) {
			if (CheckBreak(shell)) return false;
			char * name = iter->name;
			char *lname = iter->lname;
			uint32_t size = iter->size;
			uint32_t hsize = iter->hsize;
			uint16_t date = iter->date;
			uint16_t time = iter->time;
			uint8_t attr = iter->attr;

			/* output the file */
			if (optB) {
				// this overrides pretty much everything
				if (strcmp(".",uselfn&&!optZ?lname:name) && strcmp("..",uselfn&&!optZ?lname:name)) {
					int m=shell->WriteOut_NoParsing(uselfn&&!optZ?lname:name, true);
					shell->WriteOut("\n");
					if (optP) {
						p_count+=m+1;
						if (p_count%GetPauseCount()<m+1) {
							shell->WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
							uint8_t c;uint16_t n=1;
							DOS_ReadFile(STDIN,&c,&n);
							if (c==3) return false;
							if (c==0) DOS_ReadFile(STDIN,&c,&n); // read extended key
							p_count=0;
						}
					}
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
							uint8_t c;uint16_t n=1;
							DOS_ReadFile(STDIN,&c,&n);
							if (c==3) return false;
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
				uint8_t day	= (uint8_t)(date & 0x001f);
				uint8_t month	= (uint8_t)((date >> 5) & 0x000f);
				uint16_t year = (uint16_t)((date >> 9) + 1980);
				uint8_t hour	= (uint8_t)((time >> 5 ) >> 6);
				uint8_t minute = (uint8_t)((time >> 5) & 0x003f);
				unsigned int m=0;
				if (attr & DOS_ATTR_DIRECTORY) {
					if (optW) {
						shell->WriteOut("[%s]",name);
						size_t namelen = strlen(name);
						if (namelen <= 14) {
							for (size_t i=14-namelen;i>0;i--) shell->WriteOut(" ");
						}
					} else {
						shell->WriteOut("%-8s %-3s   %-16s %s %s",name,ext,"<DIR>",FormatDate(year,month,day),FormatTime(hour,minute,100,100));
                        if (uselfn&&!optZ) {
                            shell->WriteOut(" ");
                            m=shell->WriteOut_NoParsing(lname, true);
                        }
                        shell->WriteOut("\n");
                        if (optP) {
                            p_count+=(optW?5:1)*m;
                            if (p_count%(GetPauseCount()*w_size)<m) {
                                shell->WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
                                uint8_t c;uint16_t n=1;
                                DOS_ReadFile(STDIN,&c,&n);
                                if (c==3) return false;
                                if (c==0) DOS_ReadFile(STDIN,&c,&n); // read extended key
                                p_count=0;
                            }
                        }
					}
					dir_count++;
				} else {
					if (optW) {
						shell->WriteOut("%-16s",name);
					} else {
						FormatNumber(size+hsize*0x100000000,numformat);
						shell->WriteOut("%-8s %-3s   %16s %s %s",name,ext,numformat,FormatDate(year,month,day),FormatTime(hour,minute,100,100));
                        if (uselfn&&!optZ) {
                            shell->WriteOut(" ");
                            m=shell->WriteOut_NoParsing(lname, true);
                        }
                        shell->WriteOut("\n");
                        if (optP) {
                            p_count+=(optW?5:1)*m;
                            if (p_count%(GetPauseCount()*w_size)<m) {
                                shell->WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
                                uint8_t c;uint16_t n=1;
                                DOS_ReadFile(STDIN,&c,&n);
                                if (c==3) return false;
                                if (c==0) DOS_ReadFile(STDIN,&c,&n); // read extended key
                                p_count=0;
                            }
                        }
					}
					if (optS) {
						cfile_count++;
						cbyte_count+=size+hsize*0x100000000;
					}
					file_count++;
					byte_count+=size+hsize*0x100000000;
				}
				if (optW) w_count++;
			}
			if (optW && w_count%5==0 && tcols>80) shell->WriteOut("\n");
			if (optP && !optB && !(++p_count%(GetPauseCount()*w_size))) {
				if (optW&&w_count%5) {shell->WriteOut("\n");w_count=0;}
				shell->WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
				uint8_t c;uint16_t n=1;
				DOS_ReadFile(STDIN,&c,&n);
				if (c==3) return false;
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
		if (!dirPaused(shell, w_size, optP, optW, false)) return false;
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
				dta.GetResult(result.name,result.lname,result.size,result.hsize,result.date,result.time,result.attr);

				if(result.attr&DOS_ATTR_DIRECTORY && strcmp(result.name, ".")&&strcmp(result.name, "..")) {
					strcat(sargs, result.name);
					strcat(sargs, "\\");
					char *fname = strrchr_dbcs(args, '\\');
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
			if (!dirPaused(shell, w_size, optP, optW, false)) return false;
		}
	}
	return true;
}

void DOS_Shell::CMD_DIR(char * args) {
	HELP("DIR");
	char numformat[64];
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
	bool optAminusD=ScanCMDBool(args,"A-D")||ScanCMDBool(args,"A:-D");
	bool optAS=ScanCMDBool(args,"AS")||ScanCMDBool(args,"A:S");
	bool optAminusS=ScanCMDBool(args,"A-S")||ScanCMDBool(args,"A:-S");
	bool optAH=ScanCMDBool(args,"AH")||ScanCMDBool(args,"A:H");
	bool optAminusH=ScanCMDBool(args,"A-H")||ScanCMDBool(args,"A:-H");
	bool optAR=ScanCMDBool(args,"AR")||ScanCMDBool(args,"A:R");
	bool optAminusR=ScanCMDBool(args,"A-R")||ScanCMDBool(args,"A:-R");
	bool optAA=ScanCMDBool(args,"AA")||ScanCMDBool(args,"A:A");
	bool optAminusA=ScanCMDBool(args,"A-A")||ScanCMDBool(args,"A:-A");
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
	bool reverseSort = false, rev2Sort = false;
	bool optON=ScanCMDBool(args,"ON")||ScanCMDBool(args,"O:N");
	if (ScanCMDBool(args,"O-N")||ScanCMDBool(args,"O:-N")) {
		optON = true;
		reverseSort = true;
	}
	bool optOD=ScanCMDBool(args,"OD")||ScanCMDBool(args,"O:D");
	if (ScanCMDBool(args,"O-D")||ScanCMDBool(args,"O:-D")) {
		optOD = true;
		reverseSort = true;
	}
	bool optOE=ScanCMDBool(args,"OE")||ScanCMDBool(args,"O:E");
	if (ScanCMDBool(args,"O-E")||ScanCMDBool(args,"O:-E")) {
		optOE = true;
		reverseSort = true;
	}
	bool optOS=ScanCMDBool(args,"OS")||ScanCMDBool(args,"O:S");
	if (ScanCMDBool(args,"O-S")||ScanCMDBool(args,"O:-S")) {
		optOS = true;
		reverseSort = true;
	}
	bool optOG=ScanCMDBool(args,"OG")||ScanCMDBool(args,"O:G");
	if (ScanCMDBool(args,"O-G")||ScanCMDBool(args,"O:-G")) {
		optOG = true;
		reverseSort = true;
	}
	bool b0 = false, b1 = false, b2 = false, b3 = false;
	bool optOGN = false, optOGD = false, optOGE = false, optOGS = false;
	b0=ScanCMDBool(args,"O")||ScanCMDBool(args,"OGN")||ScanCMDBool(args,"O:GN");b1=ScanCMDBool(args,"O-GN")||ScanCMDBool(args,"O:-GN");
	b2=ScanCMDBool(args,"O-G-N")||ScanCMDBool(args,"O:-G-N"),b3=ScanCMDBool(args,"OG-N")||ScanCMDBool(args,"O:G-N");
	if (b0||b1||b2||b3) {
		optOGN = true;
		reverseSort = b1||b2;
		rev2Sort = b2||b3;
	}
	b0=ScanCMDBool(args,"OGD")||ScanCMDBool(args,"O:GD");b1=ScanCMDBool(args,"O-GD")||ScanCMDBool(args,"O:-GD");
	b2=ScanCMDBool(args,"O-G-D")||ScanCMDBool(args,"O:-G-D");b3=ScanCMDBool(args,"OG-D")||ScanCMDBool(args,"O:G-D");
	if (b0||b1||b2||b3) {
		optOGD = true;
		reverseSort = b1||b2;
		rev2Sort = b2||b3;
	}
	b0=ScanCMDBool(args,"OGE")||ScanCMDBool(args,"O:GE");b1=ScanCMDBool(args,"O-GE")||ScanCMDBool(args,"O:-GE");
	b2=ScanCMDBool(args,"O-G-E")||ScanCMDBool(args,"O:-G-E");b3=ScanCMDBool(args,"OG-E")||ScanCMDBool(args,"O:G-E");
	if (b0||b1||b2||b3) {
		optOGE = true;
		reverseSort = b1||b2;
		rev2Sort = b2||b3;
	}
	b0=ScanCMDBool(args,"OGS")||ScanCMDBool(args,"O:GS");b1=ScanCMDBool(args,"O-GS")||ScanCMDBool(args,"O:-GS");
	b2=ScanCMDBool(args,"O-G-S")||ScanCMDBool(args,"O:-G-S");b3=ScanCMDBool(args,"OG-S")||ScanCMDBool(args,"O:G-S");
	if (b0||b1||b2||b3) {
		optOGS = true;
		reverseSort = b1||b2;
		rev2Sort = b2||b3;
	}
	if (optOGN||optOGD||optOGE||optOGS) optOG = false;
	if (ScanCMDBool(args,"-O")) {
		optOG = false;
		optON = false;
		optOD = false;
		optOE = false;
		optOS = false;
		optOGN = false;
		optOGD = false;
		optOGE = false;
		optOGS = false;
		reverseSort = false;
	}
	const char *valid[] = {"4","W","P","-W","-P","WP","PW","Z","-Z","S","-S","B","-B",
	"A","-A","AD","A:D","A-D","A:-D","AS","A:S","A-S","A:-S","AH","A:H","A-H","A:-H","AR","A:R","A-R","A:-R","AA","A:A","A-A","A:-A",
	"O","-O","ON","O:N","O-N","O:-N","OD","O:D","O-D","O:-D","OE","O:E","O-E","O:-E","OS","O:S","O-S","O:-S","OG","O:G","O-G","O:-G",
	"OGN","O:GN","O-GN","O:-GN","OG-N","O:G-N","O-G-N","O:-G-N","OGD","O:GD","O-GD","O:-GD","OG-D","O:G-D","O-G-D","O:-G-D",
	"OGE","O:GE","O-GE","O:-GE","OG-E","O:G-E","O-G-E","O:-G-E","OGS","O:GS","O-GS","O:-GS","OG-S","O:G-S","O-G-S","O:-G-S",
	NULL};
	if (args && strlen(args)>1) for (int i=0; valid[i] && *args && strchr(args,'/'); i++) while (ScanCMDBool(args,valid[i]));
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
		// handle \, C:\, etc.                          handle C:, etc.
		if(check_last_split_char(args, argLen, '\\') || args[argLen-1] == ':') {
			strcat(args,"*.*");
		}
	}
	args = ExpandDot(args,buffer,CROSS_LEN,!uselfn);

	if (DOS_FindDevice(args) != DOS_DEVICES) {
		WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
		return;
	}
	if (!strrchr(args,'*') && !strrchr(args,'?')) {
		uint16_t attribute=0;
		if(!DOS_GetSFNPath(args,sargs,false)) {
            if (strlen(args)>1&&toupper(args[0])>='A'&&toupper(args[0])<='Z'&&args[1]==':'&&!Drives[toupper(args[0])-'A'])
                WriteOut(MSG_Get("SHELL_ILLEGAL_DRIVE"));
            else
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
		if (strlen(args)>1&&toupper(args[0])>='A'&&toupper(args[0])<='Z'&&args[1]==':'&&!Drives[toupper(args[0])-'A'])
            WriteOut(MSG_Get("SHELL_ILLEGAL_DRIVE"));
		else
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
	*(strrchr_dbcs(path,'\\')+1)=0;
	if (!DOS_GetSFNPath(path,sargs,true)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}
    if (*(sargs+strlen(sargs)-1) != '\\') strcat(sargs,"\\");
    if (!optB) {
#if defined(WIN32) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
		if (Network_IsNetworkResource(args)) {
			WriteOut("\n");
			if (optP) p_count+=optW?5:1;
		} else
#endif
		{
			if (strlen(sargs)>2&&sargs[1]==':') {
				char c[]=" _:";
				c[1]=toupper(sargs[0]);
				CMD_VOL(c[1]>='A'&&c[1]<='Z'?c:empty_string);
			} else
				CMD_VOL(empty_string);
			if (optP) p_count+=optW?15:3;
		}
	}

	/* Command uses dta so set it to our internal dta */
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	dirs.clear();
	dirs.emplace_back(std::string(args));
	inshell=true;
	while (!dirs.empty()) {
		ctrlbrk=false;
		if (!doDir(this, (char *)dirs.begin()->c_str(), dta, numformat, w_size, optW, optZ, optS, optP, optB, optA, optAD, optAminusD, optAS, optAminusS, optAH, optAminusH, optAR, optAminusR, optAA, optAminusA, optOGN, optOGD, optOGE, optOGS, optOG, optON, optOD, optOE, optOS, reverseSort, rev2Sort)) {dos.dta(save_dta);inshell=false;return;}
		dirs.erase(dirs.begin());
	}
	inshell=false;
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
		uint8_t drive=dta.GetSearchDrive();
		uint64_t free_space=1024u*1024u*100u;

        if(Drives[drive]) {
            uint32_t bytes_sector32; uint32_t sectors_cluster32; uint32_t total_clusters32; uint32_t free_clusters32;
            uint64_t total_clusters64; uint64_t free_clusters64;
            // Since this is the *internal* shell, we want use maximum available query capability at first
            if(Drives[drive]->AllocationInfo64(&bytes_sector32, &sectors_cluster32, &total_clusters64, &free_clusters64)) {
                freec = 0;
                free_space = (uint64_t)bytes_sector32 * (Bitu)sectors_cluster32 * (Bitu)free_clusters64;
            }
            else if((dos.version.major > 7 || (dos.version.major == 7 && dos.version.minor >= 10)) &&
                Drives[drive]->AllocationInfo32(&bytes_sector32, &sectors_cluster32, &total_clusters32, &free_clusters32)) { /* FAT32 aware extended API */
                freec = 0;
                free_space = (uint64_t)bytes_sector32 * (Bitu)sectors_cluster32 * (Bitu)free_clusters32;
            }
            else {
                uint16_t bytes_sector; uint8_t sectors_cluster; uint16_t total_clusters; uint16_t free_clusters;
                rsize = true;
                freec = 0;
                Drives[drive]->AllocationInfo(&bytes_sector, &sectors_cluster, &total_clusters, &free_clusters);
                free_space = (uint64_t)bytes_sector * (Bitu)sectors_cluster * (Bitu)(freec ? freec : free_clusters);
                rsize = false;
            }
        }
#if defined(WIN32) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
		if (Network_IsNetworkResource(args)) {
			std::string str = MSG_Get("SHELL_CMD_DIR_BYTES_FREE");
			std::string::size_type idx = str.rfind(" %");
			if (idx != std::string::npos) {
				str = str.substr(0, idx)+"\n"; // No "nnn Bytes free"
				WriteOut(str.c_str(),dir_count);
				if (!dirPaused(this, w_size, optP, optW)) {dos.dta(save_dta);return;}
			}
		} else
#endif
		{
			FormatNumber(free_space,numformat);
			WriteOut(MSG_Get("SHELL_CMD_DIR_BYTES_FREE"),dir_count,numformat);
			if (!dirPaused(this, w_size, optP, optW)) {dos.dta(save_dta);return;}
		}
	}
	dos.dta(save_dta);
}

void DOS_Shell::CMD_LS(char *args) {
	HELP("LS");
	bool optA=ScanCMDBool(args,"A");
	bool optL=ScanCMDBool(args,"L");
	bool optP=ScanCMDBool(args,"P");
	bool optZ=ScanCMDBool(args,"Z");
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	if (!outcon) optL = true;

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
	pattern = ExpandDot((char *)pattern.c_str(), buffer, sizeof(buffer), true);

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
	bool ret = DOS_FindFirst(((uselfn?"\"":"")+std::string(spattern)+(uselfn?"\"":"")).c_str(), 0xffff & ~DOS_ATTR_VOLUME);
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
		dta.GetResult(result.name, result.lname, result.size, result.hsize, result.date, result.time, result.attr);
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
	ctrlbrk=false;
	w_count = p_count = 0;

	for (const auto &entry : results) {
		if (CheckBreak(this)) {dos.dta(save_dta);return;}
		std::string name = uselfn&&!optZ?entry.lname:entry.name;
		if (name == "." || name == "..") continue;
		if (!optA && (entry.attr&DOS_ATTR_SYSTEM || entry.attr&DOS_ATTR_HIDDEN)) continue;
		if (entry.attr & DOS_ATTR_DIRECTORY) {
			if (!uselfn||optZ) upcase(name);
			if (col==1) {
				WriteOut("\033[34;1m");
				p_count+=WriteOut_NoParsing(name.c_str(), true);
				WriteOut("\033[0m\n");
				p_count++;
			} else
				WriteOut("\033[34;1m%s\033[0m%-*s", name.c_str(), max[w_count % col]-name.size(), "");
		} else {
			if (!uselfn||optZ) lowcase(name);
			bool is_executable=false;
			if (name.length()>4) {
				std::string ext=name.substr(name.length()-4);
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
				if (ext==".exe"||ext==".com"||ext==".bat") is_executable=true;
			}
			if (col==1) {
				if (is_executable) WriteOut("\033[32;1m");
				p_count+=WriteOut_NoParsing(name.c_str(), true);
				WriteOut(is_executable?"\033[0m\n":"\n");
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
			uint8_t c;uint16_t n=1;
			DOS_ReadFile(STDIN,&c,&n);
			if (c==3) {dos.dta(save_dta);return;}
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
    const char ch_y = MSG_Get("INT21_6523_YESNO_CHARS")[0];
    const char ch_n = MSG_Get("INT21_6523_YESNO_CHARS")[1];
    const char ch_Y = toupper(ch_y);
    const char ch_N = toupper(ch_n);
    const char ch_a = MSG_Get("SHELL_ALLFILES_CHAR")[0];
    const char ch_A = toupper(ch_a);
	StripSpaces(args);
	/* Command uses dta so set it to our internal dta */
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	uint32_t size,hsize;uint16_t date;uint16_t time;uint8_t attr;
	char name[DOS_NAMELENGTH_ASCII], lname[LFN_NAMELENGTH+1];
	std::vector<copysource> sources;
	// ignore /b and /t switches: always copy binary
	while(ScanCMDBool(args,"B")) ;
	while(ScanCMDBool(args,"T")) ; //Shouldn't this be A ?
	while(ScanCMDBool(args,"A")) ;
	bool optY=ScanCMDBool(args,"Y");
	if (bf||call||exec) optY=true;
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
	// Gather all sources (extension to copy more than 1 file specified at command line)
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
					find_last=strrchr_dbcs(source_x,'\\');
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
                    dta.GetResult(name,lname,size,hsize,date,time,attr);
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
	// If more than one object exists and last target is not part of a 
	// concat sequence then make it the target.
	if(sources.size()>1 && !sources[sources.size()-2].concat){
		target = sources.back();
		sources.pop_back();
	}
	//If no target => default target with concat flag true to detect a+b+c
	if(target.filename.size() == 0) target = copysource(defaulttarget,true);

	copysource oldsource;
	copysource source;
	uint32_t count = 0;
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
		char* pos = strrchr_dbcs(pathSource,'\\');
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
				dta.GetResult(name,lname,size,hsize,date,time,attr);
				if (attr & DOS_ATTR_DIRECTORY) {
					strcat(pathTarget,"\\");
					target_is_file = false;
				}
			}
		} else target_is_file = false;

		//Find first sourcefile
		char sPath[DOS_PATHLENGTH];
		bool ret=DOS_GetSFNPath(source.filename.c_str(),sPath,false) && DOS_FindFirst(((strchr(sPath, ' ')&&sPath[0]!='"'?"\"":"")+std::string(sPath)+(strchr(sPath, ' ')&&sPath[strlen(sPath)-1]!='"'?"\"":"")).c_str(),0xffff & ~DOS_ATTR_VOLUME);
		if (!ret) {
			WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),source.filename.c_str());
			dos.dta(save_dta);
			return;
		}

		uint16_t sourceHandle,targetHandle = 0;
		char nameTarget[DOS_PATHLENGTH];
		char nameSource[DOS_PATHLENGTH], nametmp[DOS_PATHLENGTH+2];
		
		// Cache so we don't have to recalculate
		size_t pathTargetLen = strlen(pathTarget);
		
		// See if we have to substitute filename or extension
		char * ext = nullptr;
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
						size_t lastSlash = std::string::npos;
						bool lead = false;
						for (unsigned int i=0; i<source.filename.size(); i++) {
							if (lead) lead = false;
							else if ((IS_PC98_ARCH && shiftjis_lead_byte(source.filename[i])) || (isDBCSCP() && isKanji1(source.filename[i]))) lead = true;
							else if (source.filename[i]=='\\') lastSlash = i;
						}
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
					ext = nullptr;
				}
			}
		}

		bool echo=dos.echo, second_file_of_current_source = false;
		ctrlbrk=false;
		while (ret) {
			if (CheckBreak(this)) {
				dos.dta(save_dta);
				DOS_CloseFile(sourceHandle);
				if (targetHandle)
					DOS_CloseFile(targetHandle);
				return;
			}
			dta.GetResult(name,lname,size,hsize,date,time,attr);

			if ((attr & DOS_ATTR_DIRECTORY)==0) {
                uint16_t ftime,fdate;

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
						WriteOut(MSG_Get("SHELL_CMD_COPY_NOSELF"));
						dos.dta(save_dta);
						DOS_CloseFile(sourceHandle);
						if (targetHandle)
							DOS_CloseFile(targetHandle);
						return;
						}
					uint16_t fattr;
					bool exist = DOS_GetFileAttr(nameTarget, &fattr);
					if (!(attr & DOS_ATTR_DIRECTORY) && DOS_FindDevice(nameTarget) == DOS_DEVICES) {
						if (exist && !optY && !oldsource.concat) {
							dos.echo=false;
							WriteOut(MSG_Get("SHELL_CMD_COPY_CONFIRM"), nameTarget);
							uint8_t c;
							uint16_t n=1;
							while (true)
								{
								DOS_ReadFile (STDIN,&c,&n);
								if (c==3) {dos.dta(save_dta);DOS_CloseFile(sourceHandle);dos.echo=echo;return;}
								if (c==ch_y||c==ch_Y) {WriteOut("%c\r\n", ch_Y);break;}
								if (c==ch_n||c==ch_N) {WriteOut("%c\r\n", ch_N);break;}
								if (c==ch_a||c==ch_A) {WriteOut("%c\r\n", ch_A);optY=true;break;}
								}
							if (c==ch_n||c==ch_N) {DOS_CloseFile(sourceHandle);ret = DOS_FindNext();continue;}
						}
						if (!exist&&size) {
							int drive=strlen(nameTarget)>1&&(nameTarget[1]==':'||nameTarget[2]==':')?(toupper(nameTarget[nameTarget[0]=='"'?1:0])-'A'):-1;
                            if(drive >= 0 && Drives[drive]) {
                                uint16_t bytes_sector; uint8_t sectors_cluster; uint16_t total_clusters; uint16_t free_clusters;
                                uint32_t bytes32 = 0, sectors32 = 0, clusters32 = 0, free32 = 0;
                                bool no_free_space = true;
                                rsize = true;
                                freec = 0;
                                if(dos.version.major > 7 || (dos.version.major == 7 && dos.version.minor >= 10)) {
                                    Drives[drive]->AllocationInfo32(&bytes32, &sectors32, &clusters32, &free32);
                                    no_free_space = (uint64_t)bytes32 * (uint64_t)sectors32 * (uint64_t)free32 < size ? true : false;
                                    //LOG_MSG("drive=%u, no_free_space = %d bytes32=%u, sectors32=%u, free32 =%u, free_space=%u, size=%u",
                                    //  drive, no_free_space ? 1 : 0, bytes32, sectors32, free32, bytes32*sectors32*free32, size);
                                }
                                if(bytes32 == 0 || sectors32 == 0 || dos.version.major < 7 || (dos.version.major == 7 && dos.version.minor < 10)) {
                                    Drives[drive]->AllocationInfo(&bytes_sector, &sectors_cluster, &total_clusters, &free_clusters);
                                    no_free_space = (Bitu)bytes_sector* (Bitu)sectors_cluster* (Bitu)(freec ? freec : free_clusters) < size ? true : false;
                                    //LOG_MSG("no_free_space = %d bytes=%u, sectors=%u, free =%u, free_space=%u, size=%u",
                                    // no_free_space ? 1 : 0, bytes_sector, sectors_cluster, freec, bytes_sector*sectors_cluster*free_clusters, size);
                                }
                                rsize = false;
								if (no_free_space) {
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
						uint32_t dummy=0;

                        if (DOS_FindDevice(name) == DOS_DEVICES && !DOS_SetFileDate(targetHandle, ftime, fdate))
                            LOG_MSG("WARNING: COPY unable to apply date/time to dest");

						//In concat mode. Open the target and seek to the eof
						if (!oldsource.concat || (DOS_OpenFile(nameTarget,OPEN_READWRITE,&targetHandle) && 
					        	                  DOS_SeekFile(targetHandle,&dummy,DOS_SEEK_END))) {
							// Copy 
							static uint8_t buffer[0x8000]; // static, otherwise stack overflow possible.
							bool	failed = false;
							uint16_t	toread = 0x8000;
							bool iscon=DOS_FindDevice(name)==DOS_FindDevice("con");
							if (iscon) dos.echo=true;
							bool cont;
							do {
								if (!DOS_ReadFile(sourceHandle,buffer,&toread)) failed=true;
								if (iscon) {
									if (dos.errorcode==77) {
										dos.dta(save_dta);
										DOS_CloseFile(sourceHandle);
										DOS_CloseFile(targetHandle);
										if (!exist) DOS_UnlinkFile(nameTarget);
										dos.echo=echo;
										return;
									}
									cont=true;
									for (int i=0;i<toread;i++)
										if (buffer[i]==26) {
											toread=i;
											cont=false;
											break;
										}
									if (!DOS_WriteFile(targetHandle,buffer,&toread)) failed=true;
									if (cont) toread=0x8000;
								} else {
									if (DOS_FindDevice(nameTarget)==DOS_FindDevice("con")&&CheckBreak(this)) {failed=true;break;}
									if (!DOS_WriteFile(targetHandle,buffer,&toread)) failed=true;
									cont=toread == 0x8000;
								}
							} while (cont);
							if (!DOS_CloseFile(sourceHandle)) failed=true;
#if defined(WIN32)
							if (file_access_tries>0 && DOS_FindDevice(name) == DOS_DEVICES) DOS_SetFileDate(targetHandle, ftime, fdate);
#endif
							if (!DOS_CloseFile(targetHandle)) failed=true;
							if (failed)
                                WriteOut(MSG_Get("SHELL_CMD_COPY_ERROR"),uselfn?lname:name);
                            else if (strcmp(name,lname)&&uselfn)
                                WriteOut(" %s [%s]\n",lname,name);
                            else
                                WriteOut(" %s\n",uselfn?lname:name);
							if(!source.concat && !special && !failed) count++; //Only count concat files once
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

static void skipspc(char* &pcheck) {
	while (*pcheck != 0 && (*pcheck == ' ' || *pcheck == '\t')) pcheck++;
}

static void readnonspcu(std::string &s,char* &pcheck) {
	s.clear();
	while (*pcheck != 0 && !(*pcheck == ' ' || *pcheck == '\t')) s += toupper( *(pcheck++) );
}

static void readnonspc(std::string &s,char* &pcheck) {
	s.clear();
	while (*pcheck != 0 && !(*pcheck == ' ' || *pcheck == '\t')) s += *(pcheck++);
}

/* NTS: WARNING, this function modifies the buffer pointed to by char *args */
void DOS_Shell::CMD_SET(char * args) {
	HELP("SET");
	StripSpaces(args);

	enum op_mode_t {
		show_all_env,
		set_env,
		show_env,
		erase_env,
		first_env
	};

	op_mode_t op_mode = show_all_env;
	std::string env_name,env_value;

	const bool zdirpath = static_cast<Section_prop *>(control->GetSection("dos"))->Get_bool("drive z expand path");

	{
		std::string sw;

		/* parse arguments at the start */
		skipspc(args);
		while (*args == '/') {
			args++; /* skip / */
			readnonspcu(sw,args);

			if (sw == "P") {
				WriteOut("Set /P is not supported. Use Choice!"); /* TODO: What is SET /P supposed to do? */
				return;
			}
			else if (sw == "ERASE") { /* DOSBox-X extension: Completely erase the environment block */
				op_mode = erase_env;
			}
			else if (sw == "FIRST") { /* DOSBox-X extension: Move the specified variable to the front of the environment block */
				op_mode = first_env;
			}
			else {
				WriteOut("Unknown switch /");
				WriteOut(sw.c_str());
				WriteOut("\n");
				return;
			}

			skipspc(args);
		}
	}

	if (op_mode == first_env) {
		if (*args == 0) return;
		readnonspc(env_name,args);
	}
	else if (op_mode == show_all_env) {
		if (*args != 0) {
			/* Most SET commands take the form NAME=VALUE */
			char *p = strchr(args,'=');
			if (p == NULL) {
				/* SET <variable> without assignment prints the variable instead */
				op_mode = show_env;
				readnonspc(env_name,args);
			} else {
				/* ASCIIZ snip the args string in two, so that args is C-string name of the variable,
				 * and "p" is C-string value of the variable */
				op_mode = set_env;
				*p++ = 0; env_name = args; env_value = p;
			}
		}
	}

	switch (op_mode) {
		case show_all_env: {
			const Bitu count = GetEnvCount();
			std::string line;

			for (Bitu a = 0;a < count;a++) {
				if (GetEnvNum(a,line))
					WriteOut("%s\n",line.c_str());
			}
			break; }
		case show_env:
			if (GetEnvStr(env_name.c_str(),env_value))
				WriteOut("%s\n",env_value.c_str());
			else
				WriteOut(MSG_Get("SHELL_CMD_SET_NOT_SET"),env_name.c_str());
			break;
		case set_env:
			if (zdirpath && env_name == "path") GetExpandedPath(env_value);

			/* No parsing is needed. The command interpreter does the variable substitution for us */
			/* NTS: If Win95 is any example, the command interpreter expands the variables for us */
			if (!SetEnv(env_name.c_str(),env_value.c_str()))
				WriteOut(MSG_Get("SHELL_CMD_SET_OUT_OF_SPACE"));

			break;
		case erase_env:
			if (!EraseEnv())
				WriteOut("Unable to erase environment block\n");
			break;
		case first_env:
			if (!FirstEnv(env_name.c_str()))
				WriteOut("Unable to move environment variable\n");
			break;
		default:
			abort();
			break;
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

		uint8_t n = 0;
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
			if (!DOS_Canonicalize(word,full)) {
				if (has_not) DoCommand(args);
				return;
			}
			r=strrchr_dbcs(full, '\\');
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
			if (strchr_dbcs(word,'\"')||uselfn) {
				if (!DOS_GetSFNPath(("\""+std::string(path)+"\\").c_str(), spath, false)) strcpy(spath, path);
				if (!strlen(spath)||spath[strlen(spath)-1]!='\\') strcat(spath, "\\");
			}
			RealPt save_dta=dos.dta();
			dos.dta(dos.tables.tempdta);
			int fbak=lfn_filefind_handle;
			lfn_filefind_handle=uselfn?LFN_FILEFIND_INTERNAL:LFN_FILEFIND_NONE;
			std::string sfull=std::string(spath)+std::string(pattern);
			bool ret=DOS_FindFirst(((uselfn&&sfull.length()&&sfull[0]!='"'?"\"":"")+sfull+(uselfn&&sfull.length()&&sfull[sfull.length()-1]!='"'?"\"":"")).c_str(),0xffff & ~(DOS_ATTR_VOLUME|DOS_ATTR_DIRECTORY));
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
	uint16_t handle;
	char * word;
	bool lead = false;
	int COLS = 80;
	if (!IS_PC98_ARCH && outcon) COLS=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
	BIOS_NCOLS;
	uint8_t page=outcon?real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE):0;
nextfile:
	word=StripArg(args);
	if (!DOS_OpenFile(word,0,&handle)) {
		WriteOut(MSG_Get(dos.errorcode==DOSERR_ACCESS_DENIED?"SHELL_CMD_FILE_ACCESS_DENIED":(dos.errorcode==DOSERR_PATH_NOT_FOUND?"SHELL_ILLEGAL_PATH":"SHELL_CMD_FILE_NOT_FOUND")),word);
		return;
	}
	ctrlbrk=false;
	uint8_t c,last,last2,last3;uint16_t n=1;
	last3=last2=last=0;
	bool iscon=DOS_FindDevice(word)==DOS_FindDevice("con");
	while (n) {
		DOS_ReadFile(handle,&c,&n);
		if (outcon && !CURSOR_POS_COL(page)) last3=last2=last=0;
		if (lead) lead=false;
		else if ((IS_PC98_ARCH || isDBCSCP())
#if defined(USE_TTF)
            && dbcs_sbcs
#endif
        ) lead = isKanji1(c) && !(TTF_using()
#if defined(USE_TTF)
        && autoboxdraw
#endif
        && CheckBoxDrawing(last3, last2, last, c));
		if (n==0 || c==0x1a) break; // stop at EOF
		if (iscon) {
			if (c==3) break;
			else if (c==13) WriteOut("\r\n");
		} else if (CheckBreak(this)) break;
		else if (outcon && lead && CURSOR_POS_COL(page) == COLS-1) WriteOut(" ");
		DOS_WriteFile(STDOUT,&c,&n);
		if (outcon) {last3=last2;last2=last;last=c;}
	}
	DOS_CloseFile(handle);
	if (*args) goto nextfile;
}

void DOS_Shell::CMD_REM(char * args) {
	HELP("REM");
}

static char PAUSED(void) {
	uint8_t c; uint16_t n=1, handle;
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
	uint8_t c,last,last2,last3;
	last3=last2=last=0;
	bool lead=false;
	uint16_t n=1;
	StripSpaces(args);
	if (IS_PC98_ARCH) {
        LINES=real_readb(0x60,0x113) & 0x01 ? 25 : 20;
        COLS=80;
        if (real_readb(0x60,0x111)) LINES--; // Function keys displayed
	} else {
		LINES=(IS_EGAVGA_ARCH?real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS):24)+1;
		COLS=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
	}
	LINES--;
	if(!*args||!strcasecmp(args, "con")) {
		while (true) {
			DOS_ReadFile (STDIN,&c,&n);
			if (lead) lead=false;
			else if ((IS_PC98_ARCH || isDBCSCP())
#if defined(USE_TTF)
                && dbcs_sbcs
#endif
            ) lead = isKanji1(c) && !(TTF_using()
#if defined(USE_TTF)
            && autoboxdraw
#endif
            && CheckBoxDrawing(last3, last2, last, c));
			if (c==3) {dos.echo=echo;return;}
			else if (n==0) {if (last!=10) WriteOut("\r\n");dos.echo=echo;return;}
			else if (c==13&&last==26) {dos.echo=echo;return;}
			else {
				if (c==10);
				else if (c==13) {
					linecount++;
					WriteOut("\r\n");
					last3=last2=last=0;
				} else if (c=='\t') {
					do {
						WriteOut(" ");
						nchars++;
					} while ( nchars < COLS && nchars % TABSIZE );
				} else {
                    if (lead && nchars == COLS-1) {
                        last3=last2=last=0;
                        nlines++;
                        nchars = 0;
                        WriteOut("\n");
                        if (nlines == LINES) {
                            WriteOut("-- More -- (%u) --",linecount);
                            if (PAUSED()==3) return;
                            WriteOut("\n");
                            nlines=0;
                        }
                    }
                    nchars++;
                    WriteOut("%c", c);
				}
				if (c == 13 || nchars >= COLS) {
					nlines++;
					nchars = 0;
					if (nlines == LINES) {
						WriteOut("-- More -- (%u) --",linecount);
						if (PAUSED()==3) return;
						WriteOut("\n");
						nlines=0;
					}
				}
				last3=last2;last2=last;last=c;
			}
		}
	}
	if (strcasecmp(args,"nul")==0) return;
	if (!*args) {
		WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
		return;
	}
	uint16_t handle;
nextfile:
	word=StripArg(args);
	if (!DOS_OpenFile(word,0,&handle)) {
		WriteOut(MSG_Get(dos.errorcode==DOSERR_ACCESS_DENIED?"SHELL_CMD_FILE_ACCESS_DENIED":(dos.errorcode==DOSERR_PATH_NOT_FOUND?"SHELL_ILLEGAL_PATH":"SHELL_CMD_FILE_NOT_FOUND")),word);
		return;
	}
	ctrlbrk=false;
	lead=false;
	last3=last2=last=0;
	nlines=0;
	do {
		n=1;
		DOS_ReadFile(handle,&c,&n);
		if (lead) lead=false;
		else if ((IS_PC98_ARCH || isDBCSCP())
#if defined(USE_TTF)
            && dbcs_sbcs
#endif
        ) lead = isKanji1(c) && !(TTF_using()
#if defined(USE_TTF)
        && autoboxdraw
#endif
        && CheckBoxDrawing(last3, last2, last, c));
		if (lead && nchars == COLS-1) {
			last3=last2=last=0;
			nlines++;
			nchars = 0;
			WriteOut("\n");
			if (nlines == LINES && usecon) {
				WriteOut("-- More -- %s (%u) --",word,linecount);
				if (PAUSED()==3) {DOS_CloseFile(handle);return;}
				WriteOut("\n");
				nlines=0;
            }
		}
		DOS_WriteFile(STDOUT,&c,&n);
		last3=last2;last2=last;last=c;
		if (c != '\t') nchars++;
		else do {
			WriteOut(" ");
			nchars++;
		} while ( nchars < COLS && nchars % TABSIZE );

		if (c == '\n') linecount++;
		if (c == '\n' || nchars >= COLS) {
			last3=last2=last=0;
			nlines++;
			nchars = 0;
			if (!usecon) {
				if (c != '\n') WriteOut("\n");
			} else if (nlines == LINES) {
				WriteOut("-- More -- %s (%u) --",word,linecount);
				if (PAUSED()==3) {DOS_CloseFile(handle);return;}
				WriteOut("\n");
				nlines=0;
			}
		}
        if (CheckBreak(this)) break;
	} while (n);
	DOS_CloseFile(handle);
	if (*args) {
		WriteOut("\n");
		if (usecon && PAUSED()==3) return;
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
	uint8_t c;uint16_t n=1;
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
		if (sync_time) {manualtime=false;mainMenu.get_item("sync_host_datetime").check(true).refresh_item(mainMenu);}
		return;
	} else if(ScanCMDBool(args,"S")) {
		sync_time=true;
		manualtime=false;
		mainMenu.get_item("sync_host_datetime").check(true).refresh_item(mainMenu);
		return;
	} else if(ScanCMDBool(args,"F")) {
		sync_time=false;
		manualtime=false;
		mainMenu.get_item("sync_host_datetime").check(false).refresh_item(mainMenu);
		return;
	}
	// check if a date was passed in command line
	char c=dos.tables.country[11], c1, c2;
	uint32_t newday,newmonth,newyear;
	int n=dos.tables.country[0]==1?sscanf(args,"%u%c%u%c%u",&newday,&c1,&newmonth,&c2,&newyear):(dos.tables.country[0]==2?sscanf(args,"%u%c%u%c%u",&newyear,&c1,&newmonth,&c2,&newday):sscanf(args,"%u%c%u%c%u",&newmonth,&c1,&newday,&c2,&newyear));
	if (n==5 && c1==c && c2==c) {
		reg_cx = static_cast<uint16_t>(newyear);
		reg_dh = static_cast<uint8_t>(newmonth);
		reg_dl = static_cast<uint8_t>(newday);

		reg_ah=0x2b; // set system date
		CALLBACK_RunRealInt(0x21);
		if(reg_al==0xff) WriteOut(MSG_Get("SHELL_CMD_DATE_ERROR"));
		return;
	}
	// display the current date
	reg_ah=0x2a; // get system date
	CALLBACK_RunRealInt(0x21);

	const char* datestring = MSG_Get("SHELL_CMD_DATE_DAYS");
	uint32_t length;
	char day[6] = {0};
	if(sscanf(datestring,"%u",&length) && (length<7) && (strlen(datestring)==((size_t)length*7+1))) {
		// date string appears valid
		for(uint32_t i = 0; i < length; i++) day[i] = datestring[reg_al*length+1+i];
	}
	bool dateonly = ScanCMDBool(args,"T");
	if(!dateonly) WriteOut(MSG_Get("SHELL_CMD_DATE_NOW"));
	if (!dateonly) WriteOut("%s ", day);
	WriteOut("%s\n",FormatDate((uint16_t)reg_cx, (uint8_t)reg_dh, (uint8_t)reg_dl));
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
		
		uint32_t ticks=(uint32_t)(((double)(loctime->tm_hour*3600+
										loctime->tm_min*60+
										loctime->tm_sec))*18.206481481);
		mem_writed(BIOS_TIMER,ticks);
		if (sync_time) {manualtime=false;mainMenu.get_item("sync_host_datetime").check(true).refresh_item(mainMenu);}
		return;
	}
	uint32_t newhour,newminute,newsecond;
	char c=dos.tables.country[13], c1, c2;
	if (sscanf(args,"%u%c%u%c%u",&newhour,&c1,&newminute,&c2,&newsecond)==5 && c1==c && c2==c) {
		reg_ch = static_cast<uint16_t>(newhour);
		reg_cl = static_cast<uint8_t>(newminute);
		reg_dx = static_cast<uint8_t>(newsecond)<<8;

		reg_ah=0x2d; // set system time
		CALLBACK_RunRealInt(0x21);
		if(reg_al==0xff) WriteOut(MSG_Get("SHELL_CMD_TIME_ERROR"));
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
		WriteOut("%u%c%02u%c%02u\n",reg_ch,dos.tables.country[13],reg_cl,dos.tables.country[13],reg_dh);
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
		CommandLine command(nullptr, args);
		if (!command.GetCount()) {
			char name[DOS_NAMELENGTH_ASCII],lname[LFN_NAMELENGTH];
			uint32_t size,hsize;uint16_t date;uint16_t time;uint8_t attr;
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
					dta.GetResult(name,lname,size,hsize,date,time,attr);
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

        uint8_t drive;char dir[DOS_PATHLENGTH+2],fulldir[DOS_PATHLENGTH];
        if (strchr(arg.c_str(),'\"')==NULL)
            sprintf(dir,"\"%s\"",arg.c_str());
        else strcpy(dir,arg.c_str());
        if (!DOS_MakeName(dir,fulldir,&drive)) throw 3;
	
		localDrive * const ldp = dynamic_cast<localDrive*>(Drives[drive]);
		if (!ldp) throw 4;
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
	uint16_t umb_start=dos_infoblock.GetStartOfUMBChain();
	uint8_t umb_flag=dos_infoblock.GetUMBChainState();
	uint8_t old_memstrat=(uint8_t)(DOS_GetMemAllocStrategy()&0xff);
	if (umb_start==0x9fff) {
		if ((umb_flag&1)==0) DOS_LinkUMBsToMemChain(1);
		DOS_SetMemAllocStrategy(0x80);	// search in UMBs first
		this->ParseLine(args);
		uint8_t current_umb_flag=dos_infoblock.GetUMBChainState();
		if ((current_umb_flag&1)!=(umb_flag&1)) DOS_LinkUMBsToMemChain(umb_flag);
		DOS_SetMemAllocStrategy(old_memstrat);	// restore strategy
	} else this->ParseLine(args);
}

bool get_param(char *&args, char *&rem, char *&temp, char &wait_char, int &wait_sec)
{
	const char *last = strchr(args, 0);
	StripSpaces(args);
	temp = ScanCMDRemain(args);
	const bool optC = temp && tolower(temp[1]) == 'c';
	const bool optT = temp && tolower(temp[1]) == 't';
	if (temp && *temp && !optC && !optT)
		return false;
	if (temp) {
		if (args == temp)
			args = strchr(temp, 0) + 1;
		temp += 2;
		if (temp[0] == ':')
			temp++;
	}
	if (optC) {
		rem = temp;
	} else if (optT) {
		if (temp && *temp && *(temp + 1) == ',') {
			wait_char = *temp;
			wait_sec = atoi(temp + 2);
		} else
			wait_sec = 0;
	}
	if (args > last)
		args = NULL;
	if (args) args = trim(args);
	return true;
}

void DOS_Shell::CMD_CHOICE(char * args){
	HELP("CHOICE");
	static char defchoice[3] = {MSG_Get("INT21_6523_YESNO_CHARS")[0],MSG_Get("INT21_6523_YESNO_CHARS")[1],0};
	//char *rem1 = NULL, *rem2 = NULL; /* unused */
	char *rem = NULL, *temp = NULL, waitchar = 0, *ptr;
	int waitsec = 0;
	//bool optC = false, optT = false; /* unused */
	bool optN = ScanCMDBool(args,"N");
	bool optS = ScanCMDBool(args,"S"); //Case-sensitive matching
	// ignore /b and /m switches for compatibility
	ScanCMDBool(args,"B");
	ScanCMDBool(args,"M"); // Text
	ScanCMDBool(args,"T"); //Default Choice after timeout

	while (args && *trim(args) == '/') {
		if (!get_param(args, rem, temp, waitchar, waitsec)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), temp);
			return;
		}
	}
	if (!rem || !*rem) rem = defchoice; /* No choices specified use (national) YN */
	ptr = rem;
	uint8_t c;
	if(!optS) while ((c = (uint8_t)(*ptr))) *ptr++ = (char)toupper(c); /* When in no case-sensitive mode. make everything upcase */
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

	// TO-DO: Find out how real DOS handles /T option for making a choice after delay; use AUTOTYPE for now
	std::vector<std::string> sequence;
	bool in_char = optS ? (strchr(rem, waitchar) != nullptr) : (strchr(rem, toupper(waitchar)) || strchr(rem, tolower(waitchar)));
	if (waitchar && *rem && in_char && waitsec > 0) {
		sequence.emplace_back(std::string(1, optS?waitchar:tolower(waitchar)));
		MAPPER_AutoType(sequence, waitsec * 1000, 500, true);
	}
	uint16_t n=1;
	do {
		dotype = true;
		DOS_ReadFile (STDIN,&c,&n);
		dotype = false;
		if (n==0) {dos.return_code=255;return;}
		if (CheckBreak(this) || c==3) {dos.return_code=0;return;}
	} while (!c || !(ptr = strchr(rem,(optS?c:toupper(c)))));
	c = optS?c:(uint8_t)toupper(c);
	DOS_WriteFile (STDOUT,&c, &n);
	c = '\r'; DOS_WriteFile (STDOUT,&c, &n);
	c = '\n'; DOS_WriteFile (STDOUT,&c, &n);
	dos.return_code = (uint8_t)(ptr-rem+1);
}

static bool doAttrib(DOS_Shell * shell, char * args, DOS_DTA dta, bool optS, bool adda, bool adds, bool addh, bool addr, bool suba, bool subs, bool subh, bool subr) {
    char spath[DOS_PATHLENGTH],sargs[DOS_PATHLENGTH+4],path[DOS_PATHLENGTH+4],full[DOS_PATHLENGTH],sfull[DOS_PATHLENGTH+2];
	if (!DOS_Canonicalize(args,full)||strrchr_dbcs(full,'\\')==NULL) {
        shell->WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
        if (!optS) ctrlbrk=true;
        return false;
    }
	if (!DOS_GetSFNPath(args,spath,false)) {
		shell->WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
		if (!optS) ctrlbrk=true;
		return false;
	}
	if (!uselfn||!DOS_GetSFNPath(args,sfull,true)) strcpy(sfull,full);
    sprintf(sargs,"\"%s\"",spath);
    bool found=false, res=DOS_FindFirst(sargs,0xffff & ~DOS_ATTR_VOLUME);
	if (!res&&!optS) return false;
	//end can't be 0, but if it is we'll get a nice crash, who cares :)
	strcpy(path,full);
	*(strrchr_dbcs(path,'\\')+1)=0;
	char * end=strrchr_dbcs(full,'\\')+1;*end=0;
	char * lend=strrchr_dbcs(sfull,'\\')+1;*lend=0;
    char name[DOS_NAMELENGTH_ASCII],lname[LFN_NAMELENGTH+1];
    uint32_t size,hsize;uint16_t time,date;uint8_t attr;uint16_t fattr;
	while (res) {
		if (CheckBreak(shell)) {ctrlbrk=true;return false;}
		dta.GetResult(name,lname,size,hsize,date,time,attr);
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
						if (DOS_GetFileAttr(((uselfn||strchr(full, ' ')?(full[0]!='"'?"\"":""):"")+std::string(full)+(uselfn||strchr(full, ' ')?(full[strlen(full)-1]!='"'?"\"":""):"")).c_str(), &fattr)) {
							shell->WriteOut("  %c  %c%c%c	", fattr&DOS_ATTR_ARCHIVE?'A':' ', fattr&DOS_ATTR_SYSTEM?'S':' ', fattr&DOS_ATTR_HIDDEN?'H':' ', fattr&DOS_ATTR_READ_ONLY?'R':' ');
							shell->WriteOut_NoParsing(uselfn?sfull:full, true);
							shell->WriteOut("\n");
						}
					} else
						shell->WriteOut(MSG_Get("SHELL_CMD_ATTRIB_SET_ERROR"),uselfn?sfull:full);
				} else {
					shell->WriteOut("  %c  %c%c%c	", attra?'A':' ', attrs?'S':' ', attrh?'H':' ', attrr?'R':' ');
					shell->WriteOut_NoParsing(uselfn?sfull:full, true);
					shell->WriteOut("\n");
				}
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
				dta.GetResult(result.name,result.lname,result.size,result.hsize,result.date,result.time,result.attr);

				if((result.attr&DOS_ATTR_DIRECTORY) && strcmp(result.name, ".")&&strcmp(result.name, "..")) {
					strcat(path, result.name);
					strcat(path, "\\");
					char *fname = strrchr_dbcs(args, '\\');
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
				find_last=strrchr_dbcs(sfull,'\\');
				if (find_last==NULL) find_last=sfull;
				else find_last++;
				if (sfull[strlen(sfull)-1]=='*'&&strchr(find_last, '.')==NULL) strcat(sfull, ".*");
			}
		}
	} while (*args);

	char buffer[CROSS_LEN];
	args = ExpandDot(sfull,buffer, CROSS_LEN, false);
	StripSpaces(args);
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	adirs.clear();
	adirs.emplace_back(std::string(args));
	bool found=false;
	inshell=true;
	while (!adirs.empty()) {
		ctrlbrk=false;
		if (doAttrib(this, (char *)adirs.begin()->c_str(), dta, optS, adda, adds, addh, addr, suba, subs, subh, subr))
			found=true;
		else if (ctrlbrk)
			break;
		adirs.erase(adirs.begin());
	}
	if (!found&&!ctrlbrk) WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
	inshell=false;
	ctrlbrk=false;
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
		if (strlen(args) == 1 && *args == ';')
			*args = 0;
        if (args) {
            std::string vstr = args;
            bool zdirpath = static_cast<Section_prop *>(control->GetSection("dos"))->Get_bool("drive z expand path");
            if (zdirpath) GetExpandedPath(vstr);
            strcat(pathstring,vstr.c_str());
        }
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
			if (*word=='=') word=trim(word+1);
			if (isdigit(*word)) {
				if (*args) {
					WriteOut(MSG_Get("SHELL_INVALID_PARAMETER"), args);
					return;
				}
				if (set_ver(word))
					dos_ver_menu(false);
				else
					WriteOut(MSG_Get("SHELL_CMD_VER_INVALID"));
				return;
			}
			if (*word) {
				WriteOut(MSG_Get("SHELL_INVALID_PARAMETER"), word);
				return;
			}
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
		if (optR) WriteOut("DOSBox-X Git commit %s, built on %s\nPlatform: %s %d-bit", GIT_COMMIT_HASH, UPDATED_STR, OS_PLATFORM_LONG, OS_BIT_INT);
	}
}

void DOS_Shell::CMD_VOL(char *args){
	HELP("VOL");
	uint8_t drive=DOS_GetDefaultDrive();
	if(args && *args){
		args++;
		uint32_t argLen = (uint32_t)strlen(args);
		switch (args[argLen-1]) {
		case ':' :
			if(!strcasecmp(args,":")) return;
			int drive2; drive2= toupper(*reinterpret_cast<unsigned char*>(&args[0]));
			char * c; c = strchr(args,':'); *c = '\0';
			if (Drives[drive2-'A']) drive = drive2 - 'A';
			else {
				WriteOut(MSG_Get("SHELL_ILLEGAL_DRIVE"));
				return;
			}
			break;
		default:
			WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
			return;
		}
	}
	char const* bufin = Drives[drive]->GetLabel();
    if (tree)
        WriteOut(MSG_Get("SHELL_CMD_VOL_TREE"),bufin);
    else {
        WriteOut(MSG_Get("SHELL_CMD_VOL_DRIVE"),drive+'A');

        //if((drive+'A')=='Z') bufin="DOSBOX-X";
        if(strcasecmp(bufin,"")==0)
            WriteOut(MSG_Get("SHELL_CMD_VOL_SERIAL_NOLABEL"));
        else
            WriteOut(MSG_Get("SHELL_CMD_VOL_SERIAL_LABEL"),bufin);
    }

	WriteOut(tree?MSG_Get("SHELL_CMD_VOL_SERIAL")+1:MSG_Get("SHELL_CMD_VOL_SERIAL"));
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
	bool optH=ScanCMDBool(args,"H");
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
	uint8_t drive;
	if (DOS_MakeName(name, fullname, &drive)) {
        if (optH) {
           if (!strncmp(Drives[drive]->GetInfo(),"local ",6) || !strncmp(Drives[drive]->GetInfo(),"CDRom ",6)) {
               localDrive *ldp = dynamic_cast<localDrive*>(Drives[drive]);
               Overlay_Drive *odp = dynamic_cast<Overlay_Drive*>(Drives[drive]);
               std::string hostname = "";
               if (odp) hostname = odp->GetHostName(fullname);
               else if (ldp) hostname = ldp->GetHostName(fullname);
               if (hostname.size()) {
                   WriteOut_NoParsing(hostname.c_str(), true);
                   WriteOut("\n");
               }
           }
        } else
#if defined(WIN32) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
            if (Network_IsNetworkResource(fullname)) {
                WriteOut_NoParsing(name, true);
                WriteOut("\r\n");
            } else
#endif
            {
                WriteOut("%c:\\", drive+'A');
                WriteOut_NoParsing(fullname, true);
                WriteOut("\r\n");
            }
    }
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
	//HELP("ADDKEY");
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
extern bool tohide;
bool debugger_break_on_exec = false;
void DEBUG_Enable_Handler(bool pressed);
void DOS_Shell::CMD_DEBUGBOX(char * args) {
    while (*args == ' ') args++;
	std::string argv=std::string(args);
	args=StripArg(args);
	HELP("DEBUGBOX");
    /* TODO: The command as originally taken from DOSBox SVN supported a /NOMOUSE option to remove the INT 33h vector */
    if (!*args) {
        tohide=false;
        DEBUG_Enable_Handler(true);
        tohide=true;
        return;
    } else if (!strcmp(args,"-?")) {
		args[0]='/';
		HELP("DEBUGBOX");
		return;
	}
    debugger_break_on_exec = true;
    DoCommand((char *)argv.c_str());
    debugger_break_on_exec = false;
}
#endif

char *str_replace(const char *orig, const char *rep, const char *with) {
    char *result, *ins, *tmp;
    size_t len_rep, len_with, len_front;
    int count;

    if (!orig || !rep) return NULL;

    char* mutable_orig = strdup(orig); // Make a mutable copy of orig
    char* original_mutable_orig = mutable_orig; // Store the original address for freeing below

    len_rep = strlen(rep);
    if (len_rep == 0) return NULL;
    len_with = with?strlen(with):0;

    ins = mutable_orig;
    for (count = 0; (tmp = strstr(ins, rep)) != NULL; ++count)
        ins = tmp + len_rep;

    tmp = result = (char *)malloc(strlen(mutable_orig) + (len_with - len_rep) * count + 1);
    if (!result) return NULL;

    while (count--) {
        ins = strstr(mutable_orig, rep);
        len_front = ins - mutable_orig;
        tmp = strncpy(tmp, mutable_orig, len_front) + len_front;
        tmp = strcpy(tmp, with?with:"") + len_with;
        mutable_orig += len_front + len_rep;
    }
    strcpy(tmp, mutable_orig);
    free(original_mutable_orig);
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
			r=strrchr_dbcs(full, '\\');
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
			uint32_t size, hsize;
			uint16_t date, time;
			uint8_t attr;
			DOS_DTA dta(dos.dta());
			std::vector<std::string> sources;
			std::string tmp;
			int fbak=lfn_filefind_handle;
			lfn_filefind_handle=lfn?LFN_FILEFIND_INTERNAL:LFN_FILEFIND_NONE;
			if (DOS_FindFirst((std::string(spath)+std::string(pattern)).c_str(), ~(DOS_ATTR_VOLUME|DOS_ATTR_DIRECTORY|DOS_ATTR_DEVICE|DOS_ATTR_HIDDEN|DOS_ATTR_SYSTEM)))
				{
				dta.GetResult(name, lname, size, hsize, date, time, attr);
				tmp=std::string(path)+std::string(lfn?lname:name);
				sources.push_back(tmp);
				while (DOS_FindNext())
					{
					dta.GetResult(name, lname, size, hsize, date, time, attr);
					tmp=std::string(path)+std::string(lfn?lname:name);
					sources.push_back(tmp);
					}
				}
			lfn_filefind_handle=fbak;
			for (std::vector<std::string>::iterator source = sources.begin(); source != sources.end(); ++source)
				DoCommand(str_replace(args, s, source->c_str()));
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
        for (cmd_alias_map_t::iterator iter = cmd_alias.begin(), end = cmd_alias.end(); iter != end; ++iter) {
			if (!*args || !strcasecmp(args, iter->first.c_str()))
				WriteOut("ALIAS %s='%s'\n", iter->first.c_str(), iter->second.c_str());
        }
    } else {
        char alias_name[256] = { 0 };
        for (unsigned int offset = 0; *args && offset < sizeof(alias_name)-1; ++offset, ++args) {
            if (*args == '=') {
                char * const cmd = trim(alias_name);
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
                    cmd_alias_map_t::iterator iter = cmd_alias.find(cmd);
                    if (iter != cmd_alias.end()) WriteOut("ALIAS %s='%s'\n", iter->first.c_str(), iter->second.c_str());
                }
                break;
            } else {
                alias_name[offset] = *args;
            }
        }
    }
}

void DOS_Shell::CMD_ASSOC(char* args) {
    HELP("ASSOC");
	args = trim(args);
    if (!*args || strchr(args, '=') == NULL) {
        for (cmd_assoc_map_t::iterator iter = cmd_assoc.begin(), end = cmd_assoc.end(); iter != end; ++iter) {
			if (!*args || !strcasecmp(args, iter->first.c_str()))
				WriteOut("%s=%s\n", iter->first.c_str(), iter->second.c_str());
        }
    } else {
        char assoc_name[256] = { 0 };
        for (unsigned int offset = 0; *args && offset < sizeof(assoc_name)-1; ++offset, ++args) {
            if (*args == '=') {
                char * const cmd = trim(assoc_name);
                if (!*cmd || cmd[0] != '.') {
                    WriteOut(MSG_Get("SHELL_INVALID_PARAMETER"), cmd);
                    break;
                }
                ++args;
                args = trim(args);
                size_t args_len = strlen(args);
                if ((*args == '"' && args[args_len - 1] == '"') || (*args == '\'' && args[args_len - 1] == '\'')) {
                    args[args_len - 1] = 0;
                    ++args;
                }
                if (!*args) {
                    cmd_assoc.erase(cmd);
                } else {
                    cmd_assoc[cmd] = args;
                    cmd_assoc_map_t::iterator iter = cmd_assoc.find(cmd);
                    if (iter != cmd_assoc.end()) WriteOut("%s=%s\n", iter->first.c_str(), iter->second.c_str());
                }
                break;
            } else {
                assoc_name[offset] = *args;
            }
        }
    }
}

void DOS_Shell::CMD_HISTORY(char* args) {
    HELP("HISTORY");
    if (ScanCMDBool(args,"C"))
        l_history.clear();
    for (auto it = l_history.rbegin(); it != l_history.rend(); ++it) {
        WriteOut_NoParsing(it->c_str(), true);
        WriteOut("\n");
    }
}

void CAPTURE_StartCapture(void);
void CAPTURE_StopCapture(void);

void CAPTURE_StartWave(void);
void CAPTURE_StopWave(void);

void CAPTURE_StartMTWave(void);
void CAPTURE_StopMTWave(void);

void CAPTURE_StartOPL(void);
void CAPTURE_StopOPL(void);

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
    bool cap_opl = false;
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
        else if (!(strcmp(arg1,"/O")))
			cap_opl = true;
		else if (!(strcmp(arg1,"/-A")))
			cap_audio = false;
        else if (!(strcmp(arg1,"/-O")))
			cap_opl = false;
		else if (!(strcmp(arg1,"/M")))
			cap_mtaudio = true;
		else if (!(strcmp(arg1,"/-M")))
			cap_mtaudio = false;
		else {
			WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),arg1);
			return;
		}
    }

    if (!cap_video && !cap_audio && !cap_mtaudio && !cap_opl)
        cap_video = true;

    if (cap_video)
        CAPTURE_StartCapture();
    if (cap_audio)
        CAPTURE_StartWave();
    if (cap_mtaudio)
        CAPTURE_StartMTWave();
    if (cap_opl)
        CAPTURE_StartOPL();

    DoCommand(args);

    if (post_exit_delay_ms > 0) {
        LOG_MSG("Pausing for post exit delay (%.3f seconds)",(double)post_exit_delay_ms / 1000);

        uint32_t lasttick=GetTicks();
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
    if (cap_opl)
        CAPTURE_StopOPL();
}

void DOS_Shell::CMD_CTTY(char * args) {
	HELP("CTTY");
	/* NTS: This is written to emulate the simplistic parsing in MS-DOS 6.22 */
	uint16_t handle;
	int i;

	/* args has leading space? */
	args = trim(args);

	/* must be device */
	if (DOS_FindDevice(args) == DOS_DEVICES) {
		WriteOut("Invalid device - %s\n", args);
		return;
	}

	/* close STDIN/STDOUT/STDERR and replace with new handle */
	if (!DOS_OpenFile(args,OPEN_READWRITE,&handle)) {
		WriteOut("Unable to open device - %s\n", args);
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
	if (char* rem = ScanCMDRemain(args)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), rem);
		return;
	}
	args = trim(args);
	if (!*args) {
		WriteOut("Current country code: %d\n", countryNo);
		return;
	}
	int newCC;
	char buffer[256];
	if (sscanf(args, "%d%s", &newCC, buffer) == 1 && newCC>0) {
		countryNo = newCC;
		DOS_SetCountry(countryNo);
		return;
	}
	WriteOut("Invalid country code - %s\n", StripArg(args));
	return;
}

extern bool jfont_init, isDBCSCP();
extern Bitu DOS_LoadKeyboardLayout(const char * layoutname, int32_t codepage, const char * codepagefile);
void runRescan(const char *str), MSG_Init(), JFONT_Init(), InitFontHandle(), ShutFontHandle(), initcodepagefont(), DOSBox_SetSysMenu();
int toSetCodePage(DOS_Shell *shell, int newCP, int opt) {
    if (isSupportedCP(newCP)) {
		dos.loaded_codepage = newCP;
        int missing = 0;
#if defined(USE_TTF)
		missing = TTF_using() ? setTTFCodePage() : 0;
#endif
        if (!TTF_using()) initcodepagefont();
        if (dos.loaded_codepage==437) DOS_LoadKeyboardLayout("us", 437, "auto");
        if (opt==-1) {
            MSG_Init();
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
            mainMenu.unbuild();
            mainMenu.rebuild();
            if (!GFX_GetPreventFullscreen()) {
                if (menu.toggle) DOSBox_SetMenu(); else DOSBox_NoMenu();
            }
#endif
            DOSBox_SetSysMenu();
        }
        if (isDBCSCP()) {
            ShutFontHandle();
            InitFontHandle();
            JFONT_Init();
        }
        SetupDBCSTable();
        runRescan("-A -Q");
#if defined(USE_TTF)
        if ((opt==-1||opt==-2)&&TTF_using()) {
            Section_prop * ttf_section = static_cast<Section_prop *>(control->GetSection("ttf"));
            const char *font = ttf_section->Get_string("font");
            if (!font || !*font) {
                ttf_reset();
#if C_PRINTER
                if (printfont) UpdateDefaultPrinterFont();
#endif
            }
        }
#endif
        return missing;
    } else if (opt<1 && shell) {
       shell->WriteOut(MSG_Get("SHELL_CMD_CHCP_INVALID"), std::to_string(newCP).c_str());
    }
    return -1;
}

const char* DOS_GetLoadedLayout(void);
Bitu DOS_ChangeCodepage(int32_t codepage, const char* codepagefile);
Bitu DOS_ChangeKeyboardLayout(const char* layoutname, int32_t codepage);

void DOS_Shell::CMD_CHCP(char * args) {
	HELP("CHCP");
	args = trim(args);
	if (!*args) {
		WriteOut(MSG_Get("SHELL_CMD_CHCP_ACTIVE"), dos.loaded_codepage);
		return;
	}
    if (IS_PC98_ARCH || IS_JEGA_ARCH) {
        WriteOut("Changing code page is not supported for the PC-98 or JEGA/AX system.\n");
        return;
    }
    if (IS_DOSV || IS_J3100)
    {
        WriteOut("Changing code page is not supported for the DOS/V or J-3100 system.\n");
        return;
    }
	int32_t newCP;
	char buff[256], *r;
    int missing = 0, n = sscanf(args, "%d%s", &newCP, buff);
    auto iter = langcp_map.find(newCP);
    const char* layout_name = DOS_GetLoadedLayout();
    int32_t cp = dos.loaded_codepage;
    Bitu keyb_error;
    if(n == 1) {
        if(newCP == 932 || newCP == 936 || newCP == 949 || newCP == 950 || newCP == 951
#if defined(USE_TTF)
            || (ttf.inUse && (newCP >= 1250 && newCP <= 1258))
#endif
            ) {
            missing = toSetCodePage(this, newCP, -1);
            if(missing > -1) SwitchLanguage(cp, newCP, true);
            if(missing > 0) WriteOut(MSG_Get("SHELL_CMD_CHCP_MISSING"), missing);
        }
        else {
#if defined(USE_TTF)
            if(ttf.inUse && !isSupportedCP(newCP)) {
                WriteOut(MSG_Get("SHELL_CMD_CHCP_INVALID"), StripArg(args));
                LOG_MSG("CHCP: Codepage %d not supported for TTF output", newCP);
                return;
            }
#endif
            keyb_error = DOS_ChangeCodepage(newCP, "auto");
            if(keyb_error == KEYB_NOERROR) {
                SwitchLanguage(cp, newCP, true);
/**
                if(layout_name != NULL) {
                    keyb_error = DOS_ChangeKeyboardLayout(layout_name, cp);
                }
*/
            }
            else {
                WriteOut(MSG_Get("SHELL_CMD_CHCP_INVALID"), StripArg(args));
                return;
            }
        }
        WriteOut(MSG_Get("SHELL_CMD_CHCP_ACTIVE"), dos.loaded_codepage);
    }
    else if(n == 2 && strlen(buff)) {
        if(*buff == ':' && strchr(StripArg(args), ':')) {
            std::string name = buff + 1;
            if(name.empty() && iter != langcp_map.end()) name = iter->second;
            if(newCP == 932 || newCP == 936 || newCP == 949 || newCP == 950 || newCP == 951) {
                missing = toSetCodePage(this, newCP, -1);
                if(missing > -1) SwitchLanguage(cp, newCP, true);
                if(missing > 0) WriteOut(MSG_Get("SHELL_CMD_CHCP_MISSING"), missing);
            }
#if defined(USE_TTF)
            else if(ttf.inUse) {
                if(newCP >= 1250 && newCP <= 1258) {
                    missing = toSetCodePage(this, newCP, -1);
                    if(missing > -1) SwitchLanguage(cp, newCP, true);
                    if(missing > 0) WriteOut(MSG_Get("SHELL_CMD_CHCP_MISSING"), missing);
                }
                else if(!isSupportedCP(newCP)) {
                    WriteOut(MSG_Get("SHELL_CMD_CHCP_INVALID"), StripArg(args));
                    LOG_MSG("CHCP: Codepage %d not supported for TTF output", newCP);
                    return;
                }
            }
#endif
            else {
                keyb_error = DOS_ChangeCodepage(newCP, "auto");
                if(keyb_error == KEYB_NOERROR) {
                    if(layout_name != NULL) {
                        keyb_error = DOS_ChangeKeyboardLayout(layout_name, cp);
                    }
                }
                else
                    WriteOut(MSG_Get("SHELL_CMD_CHCP_INVALID"), StripArg(args));
            }
            if(name.size() && dos.loaded_codepage == newCP) {
                SetVal("dosbox", "language", name);
                Load_Language(name);
            }
            WriteOut(MSG_Get("SHELL_CMD_CHCP_ACTIVE"), dos.loaded_codepage);
            return;
        }
#if defined(USE_TTF)
        if(ttf.inUse) {
            if(isSupportedCP(newCP)) {
                missing = toSetCodePage(this, newCP, -1);
                if(missing > -1) SwitchLanguage(cp, newCP, true);
                if(missing > 0) WriteOut(MSG_Get("SHELL_CMD_CHCP_MISSING"), missing);
                LOG_MSG("CHCP: Loading cpi/cpx files ignored for TTF output");
            }
            else {
                WriteOut(MSG_Get("SHELL_CMD_CHCP_INVALID"), StripArg(args));
                LOG_MSG("CHCP: Codepage %d not supported for TTF output", newCP);
                return;
            }
        }
        else {
#endif
            altcp = 0;
            for(int i = 0; i < 256; i++) altcp_to_unicode[i] = 0;
            std::string cpfile = buff;
            FILE* file = fopen(cpfile.c_str(), "r"); /* should check the result */
            std::string exepath = GetDOSBoxXPath();
            if(!file && exepath.size()) file = fopen((exepath + CROSS_FILESPLIT + cpfile).c_str(), "r");
            if(file && newCP > 0 && newCP != 932 && newCP != 936 && newCP != 949 && newCP != 950 && newCP != 951) {
                altcp = newCP;
                char line[256], * l = line;
                while(fgets(line, sizeof(line), file)) {
                    l = trim(l);
                    if(!strlen(l)) continue;
                    r = strchr(l, '#');
                    if(r) *r = 0;
                    l = trim(l);
                    if(!strlen(l) || strncasecmp(l, "0x", 2)) continue;
                    r = strchr(l, ' ');
                    if(!r) r = strchr(l, '\t');
                    if(!r) continue;
                    *r = 0;
                    int ind = (int)strtol(l + 2, NULL, 16);
                    r = trim(r + 1);
                    if(ind > 0xFF || strncasecmp(r, "0x", 2)) continue;
                    int map = (int)strtol(r + 2, NULL, 16);
                    altcp_to_unicode[ind] = map;
                }
                if(file) fclose(file);
                keyb_error = DOS_ChangeCodepage(newCP, cpfile.c_str());
                if(keyb_error == KEYB_NOERROR) {
                    if(layout_name != NULL) {
                        keyb_error = DOS_ChangeKeyboardLayout(layout_name, cp);
                    }
                }
                WriteOut(MSG_Get("SHELL_CMD_CHCP_ACTIVE"), dos.loaded_codepage);
#if defined(USE_TTF)
                if(missing > 0) WriteOut(MSG_Get("SHELL_CMD_CHCP_MISSING"), missing);
#endif
            }
            else
                WriteOut(MSG_Get("SHELL_CMD_CHCP_INVALID"), StripArg(args));
            if(file) fclose(file);
#if defined(USE_TTF)
        }
#endif
    }
    else WriteOut(MSG_Get("SHELL_CMD_CHCP_INVALID"), StripArg(args));
	return;
}

void DOS_Shell::CMD_VTEXT(char *args)
{
	HELP("VTEXT");
    if (!IS_DOSV) {
        WriteOut("This command is only supported in DOS/V mode.\n");
        return;
    }
	if (char* rem = ScanCMDRemain(args)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), rem);
		return;
	}
	args = trim(args);
	if(args && *args) {
		uint8_t new_mode = 0xff;
		char *word  = StripWord(args);
		if(!strcasecmp(word, "1"))
			new_mode = 0x70;
		else if(!strcasecmp(word, "2"))
			new_mode = 0x78;
		else if(!strcasecmp(word, "0"))
			new_mode = 0x03;
		else {
			WriteOut(MSG_Get("SHELL_INVALID_PARAMETER"), word);
			return;
		}
		if(new_mode != 0xff) {
            uint16_t oldax=reg_ax;
            reg_ax = new_mode;
            CALLBACK_RunRealInt(0x10);
			if(new_mode == 0x78) new_mode = 0x70;
			reg_ax = oldax;
		}
	}
	uint8_t mode = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_MODE);
	WriteOut(MSG_Get(mode == 0x70?"SHELL_CMD_VTEXT_ON":"SHELL_CMD_VTEXT_OFF"));
}
