/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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

/* $Id: shell_cmds.cpp,v 1.93 2009-09-21 21:04:25 h-a-l-9000 Exp $ */

#include "dosbox.h"
#include "shell.h"
#include "callback.h"
#include "regs.h"
#include "../dos/drives.h"
#include "support.h"
#include "control.h"
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <vector>
#include <string>

static SHELL_Cmd cmd_list[]={
{	"DIR",		0,			&DOS_Shell::CMD_DIR,		"SHELL_CMD_DIR_HELP"},
{	"CHDIR",	1,			&DOS_Shell::CMD_CHDIR,		"SHELL_CMD_CHDIR_HELP"},
{	"ATTRIB",	1,			&DOS_Shell::CMD_ATTRIB,		"SHELL_CMD_ATTRIB_HELP"},
{	"CALL",		1,			&DOS_Shell::CMD_CALL,		"SHELL_CMD_CALL_HELP"},
{	"CD",		0,			&DOS_Shell::CMD_CHDIR,		"SHELL_CMD_CHDIR_HELP"},
{	"CHOICE",	1,			&DOS_Shell::CMD_CHOICE,		"SHELL_CMD_CHOICE_HELP"},
{	"CLS",		0,			&DOS_Shell::CMD_CLS,		"SHELL_CMD_CLS_HELP"},
{	"COPY",		0,			&DOS_Shell::CMD_COPY,		"SHELL_CMD_COPY_HELP"},
{	"DEL",		0,			&DOS_Shell::CMD_DELETE,		"SHELL_CMD_DELETE_HELP"},
{	"DELETE",	1,			&DOS_Shell::CMD_DELETE,		"SHELL_CMD_DELETE_HELP"},
{	"ERASE",	1,			&DOS_Shell::CMD_DELETE,		"SHELL_CMD_DELETE_HELP"},
{	"ECHO",		1,			&DOS_Shell::CMD_ECHO,		"SHELL_CMD_ECHO_HELP"},
{	"EXIT",		0,			&DOS_Shell::CMD_EXIT,		"SHELL_CMD_EXIT_HELP"},	
{	"GOTO",		1,			&DOS_Shell::CMD_GOTO,		"SHELL_CMD_GOTO_HELP"},
{	"HELP",		1,			&DOS_Shell::CMD_HELP,		"SHELL_CMD_HELP_HELP"},
{	"IF",		1,			&DOS_Shell::CMD_IF,			"SHELL_CMD_IF_HELP"},
{	"LOADHIGH",	1,			&DOS_Shell::CMD_LOADHIGH, 	"SHELL_CMD_LOADHIGH_HELP"},
{	"LH",		1,			&DOS_Shell::CMD_LOADHIGH,	"SHELL_CMD_LOADHIGH_HELP"},
{	"MKDIR",	1,			&DOS_Shell::CMD_MKDIR,		"SHELL_CMD_MKDIR_HELP"},
{	"MD",		0,			&DOS_Shell::CMD_MKDIR,		"SHELL_CMD_MKDIR_HELP"},
{	"PATH",		1,			&DOS_Shell::CMD_PATH,		"SHELL_CMD_PATH_HELP"},
{	"PAUSE",	1,			&DOS_Shell::CMD_PAUSE,		"SHELL_CMD_PAUSE_HELP"},
{	"RMDIR",	1,			&DOS_Shell::CMD_RMDIR,		"SHELL_CMD_RMDIR_HELP"},
{	"RD",		0,			&DOS_Shell::CMD_RMDIR,		"SHELL_CMD_RMDIR_HELP"},
{	"REM",		1,			&DOS_Shell::CMD_REM,		"SHELL_CMD_REM_HELP"},
{	"RENAME",	1,			&DOS_Shell::CMD_RENAME,		"SHELL_CMD_RENAME_HELP"},
{	"REN",		0,			&DOS_Shell::CMD_RENAME,		"SHELL_CMD_RENAME_HELP"},
{	"SET",		1,			&DOS_Shell::CMD_SET,		"SHELL_CMD_SET_HELP"},
{	"SHIFT",	1,			&DOS_Shell::CMD_SHIFT,		"SHELL_CMD_SHIFT_HELP"},
{	"SUBST",	1,			&DOS_Shell::CMD_SUBST,		"SHELL_CMD_SUBST_HELP"},
{	"TYPE",		0,			&DOS_Shell::CMD_TYPE,		"SHELL_CMD_TYPE_HELP"},
{	"VER",		0,			&DOS_Shell::CMD_VER,		"SHELL_CMD_VER_HELP"},
{0,0,0,0}
}; 

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

static char* ExpandDot(char*args, char* buffer) {
	if(*args == '.') {
		if(*(args+1) == 0){
			strcpy(buffer,"*.*");
			return buffer;
		}
		if( (*(args+1) != '.') && (*(args+1) != '\\') ) {
			buffer[0] = '*';
			buffer[1] = 0;
			strcat(buffer,args);
			return buffer;
		} else
			strcpy (buffer, args);
	}
	else strcpy(buffer,args);
	return buffer;
}



bool DOS_Shell::CheckConfig(char* cmd_in,char*line) {
	Section* test = control->GetSectionFromProperty(cmd_in);
	if(!test) return false;
	if(line && !line[0]) {
		std::string val = test->GetPropValue(cmd_in);
		if(val != NO_SUCH_PROPERTY) WriteOut("%s\n",val.c_str());
		return true;
	}
	char newcom[1024]; newcom[0] = 0; strcpy(newcom,"z:\\config ");
	strcat(newcom,test->GetName());	strcat(newcom," ");
	strcat(newcom,cmd_in);strcat(newcom,line);
	DoCommand(newcom);
	return true;
}

void DOS_Shell::DoCommand(char * line) {
/* First split the line into command and arguments */
	line=trim(line);
	char cmd_buffer[CMD_MAXLINE];
	char * cmd_write=cmd_buffer;
	while (*line) {
		if (*line==32) break;
		if (*line=='/') break;
		if (*line=='\t') break;
		if (*line=='=') break;
		if ((*line=='.') ||(*line =='\\')) {  //allow stuff like cd.. and dir.exe cd\kees
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
	if (strlen(cmd_buffer)==0) return;
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
	if(Execute(cmd_buffer,line)) return;
	if(CheckConfig(cmd_buffer,line)) return;
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

void DOS_Shell::CMD_CLS(char * args) {
	HELP("CLS");
	reg_ax=0x0003;
	CALLBACK_RunRealInt(0x10);
}

void DOS_Shell::CMD_DELETE(char * args) {
	HELP("DELETE");
	/* Command uses dta so set it to our internal dta */
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);

	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	/* If delete accept switches mind the space infront of them. See the dir /p code */ 

	char full[DOS_PATHLENGTH];
	char buffer[CROSS_LEN];
	args = ExpandDot(args,buffer);
	StripSpaces(args);
	if (!DOS_Canonicalize(args,full)) { WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));return; }
//TODO Maybe support confirmation for *.* like dos does.	
	bool res=DOS_FindFirst(args,0xffff & ~DOS_ATTR_VOLUME);
	if (!res) {
		WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"),args);
		dos.dta(save_dta);
		return;
	}
	//end can't be 0, but if it is we'll get a nice crash, who cares :)
	char * end=strrchr(full,'\\')+1;*end=0;
	char name[DOS_NAMELENGTH_ASCII];Bit32u size;Bit16u time,date;Bit8u attr;
	DOS_DTA dta(dos.dta());
	while (res) {
		dta.GetResult(name,size,date,time,attr);	
		if (!(attr & (DOS_ATTR_DIRECTORY|DOS_ATTR_READ_ONLY))) {
			strcpy(end,name);
			if (!DOS_UnlinkFile(full)) WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"),full);
		}
		res=DOS_FindNext();
	}
	dos.dta(save_dta);
}

void DOS_Shell::CMD_HELP(char * args){
	HELP("HELP");
	bool optall=ScanCMDBool(args,"ALL");
	/* Print the help */
	if(!optall) WriteOut(MSG_Get("SHELL_CMD_HELP"));
	Bit32u cmd_index=0,write_count=0;
	while (cmd_list[cmd_index].name) {
		if (optall || !cmd_list[cmd_index].flags) {
			WriteOut("<\033[34;1m%-8s\033[0m> %s",cmd_list[cmd_index].name,MSG_Get(cmd_list[cmd_index].help));
			if(!(++write_count%22)) CMD_PAUSE(empty_string);
		}
		cmd_index++;
	}
}

void DOS_Shell::CMD_RENAME(char * args){
	HELP("RENAME");
	StripSpaces(args);
	if(!*args) {SyntaxError();return;}
	if((strchr(args,'*')!=NULL) || (strchr(args,'?')!=NULL) ) { WriteOut(MSG_Get("SHELL_CMD_NO_WILD"));return;}
	char * arg1=StripWord(args);
	char* slash = strrchr(arg1,'\\');
	if(slash) { 
		slash++;
		/* If directory specified (crystal caves installer)
		 * rename from c:\X : rename c:\abc.exe abc.shr. 
		 * File must appear in C:\ */ 
		
		char dir_source[DOS_PATHLENGTH]={0};
		//Copy first and then modify, makes GCC happy
		strcpy(dir_source,arg1);
		char* dummy = strrchr(dir_source,'\\');
		*dummy=0;

		if((strlen(dir_source) == 2) && (dir_source[1] == ':')) 
			strcat(dir_source,"\\"); //X: add slash

		char dir_current[DOS_PATHLENGTH + 1];
		dir_current[0] = '\\'; //Absolute addressing so we can return properly
		DOS_GetCurrentDir(0,dir_current + 1);
		if(!DOS_ChangeDir(dir_source)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			return;
		}
		DOS_Rename(slash,args);
		DOS_ChangeDir(dir_current);
	} else {
		DOS_Rename(arg1,args);
	}
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
		LOG(LOG_MISC,LOG_WARN)("Hu ? carriage return allready present. Is this possible?");
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
	if (!*args) {
		Bit8u drive=DOS_GetDefaultDrive()+'A';
		char dir[DOS_PATHLENGTH];
		DOS_GetCurrentDir(0,dir);
		WriteOut("%c:\\%s\n",drive,dir);
	} else if(strlen(args) == 2 && args[1]==':') {
		WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT"),toupper(*reinterpret_cast<unsigned char*>(&args[0])));
	} else 	if (!DOS_ChangeDir(args)) {
		/* Changedir failed. Check if the filename is longer then 8 and/or contains spaces */
	   
		std::string temps(args),slashpart;
		std::string::size_type separator = temps.find_first_of("\\/");
		if(!separator) {
			slashpart = temps.substr(0,1);
			temps.erase(0,1);
		}
		separator = temps.find_first_of("\\/");
		if(separator != std::string::npos) temps.erase(separator);
		separator = temps.rfind('.');
		if(separator != std::string::npos) temps.erase(separator);
		separator = temps.find(' ');
		if(separator != std::string::npos) {/* Contains spaces */
			temps.erase(separator);
			if(temps.size() >6) temps.erase(6);
			temps += "~1";
			WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT_2"),temps.insert(0,slashpart).c_str());
		} else if (temps.size()>8) {
			temps.erase(6);
			temps += "~1";
			WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT_2"),temps.insert(0,slashpart).c_str());
		} else {
			Bit8u drive=DOS_GetDefaultDrive()+'A';
			if (drive=='Z') {
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
	if (!DOS_MakeDir(args)) {
		WriteOut(MSG_Get("SHELL_CMD_MKDIR_ERROR"),args);
	}
}

void DOS_Shell::CMD_RMDIR(char * args) {
	HELP("RMDIR");
	StripSpaces(args);
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	if (!DOS_RemoveDir(args)) {
		WriteOut(MSG_Get("SHELL_CMD_RMDIR_ERROR"),args);
	}
}

static void FormatNumber(Bitu num,char * buf) {
	Bitu numm,numk,numb,numg;
	numb=num % 1000;
	num/=1000;
	numk=num % 1000;
	num/=1000;
	numm=num % 1000;
	num/=1000;
	numg=num;
	if (numg) {
		sprintf(buf,"%d,%03d,%03d,%03d",numg,numm,numk,numb);
		return;
	};
	if (numm) {
		sprintf(buf,"%d,%03d,%03d",numm,numk,numb);
		return;
	};
	if (numk) {
		sprintf(buf,"%d,%03d",numk,numb);
		return;
	};
	sprintf(buf,"%d",numb);
}	

void DOS_Shell::CMD_DIR(char * args) {
	HELP("DIR");
	char numformat[16];
	char path[DOS_PATHLENGTH];

	std::string line;
	if(GetEnvStr("DIRCMD",line)){
		std::string::size_type idx = line.find('=');
		std::string value=line.substr(idx +1 , std::string::npos);
		line = std::string(args) + " " + value;
		args=const_cast<char*>(line.c_str());
	}
   
	bool optW=ScanCMDBool(args,"W");
	ScanCMDBool(args,"S");
	bool optP=ScanCMDBool(args,"P");
	if (ScanCMDBool(args,"WP") || ScanCMDBool(args,"PW")) {
		optW=optP=true;
	}
	bool optB=ScanCMDBool(args,"B");
	bool optAD=ScanCMDBool(args,"AD");
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	Bit32u byte_count,file_count,dir_count;
	Bitu w_count=0;
	Bitu p_count=0;
	Bitu w_size = optW?5:1;
	byte_count=file_count=dir_count=0;

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
	args = ExpandDot(args,buffer);

	if (!strrchr(args,'*') && !strrchr(args,'?')) {
		Bit16u attribute=0;
		if(DOS_GetFileAttr(args,&attribute) && (attribute&DOS_ATTR_DIRECTORY) ) {
			strcat(args,"\\*.*");	// if no wildcard and a directory, get its files
		}
	}
	if (!strrchr(args,'.')) {
		strcat(args,".*");	// if no extension, get them all
	}

	/* Make a full path in the args */
	if (!DOS_Canonicalize(args,path)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}
	*(strrchr(path,'\\')+1)=0;
	if (!optB) WriteOut(MSG_Get("SHELL_CMD_DIR_INTRO"),path);

	/* Command uses dta so set it to our internal dta */
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	bool ret=DOS_FindFirst(args,0xffff & ~DOS_ATTR_VOLUME);
	if (!ret) {
		if (!optB) WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
		dos.dta(save_dta);
		return;
	}
 
	do {    /* File name and extension */
		char name[DOS_NAMELENGTH_ASCII];Bit32u size;Bit16u date;Bit16u time;Bit8u attr;
		dta.GetResult(name,size,date,time,attr);

		/* Skip non-directories if option AD is present */
		if(optAD && !(attr&DOS_ATTR_DIRECTORY) ) continue;
		
		/* output the file */
		if (optB) {
			// this overrides pretty much everything
			if (strcmp(".",name) && strcmp("..",name)) {
				WriteOut("%s\n",name);
			}
		} else {
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
					WriteOut("[%s]",name);
					size_t namelen = strlen(name);
					if (namelen <= 14) {
						for (size_t i=14-namelen;i>0;i--) WriteOut(" ");
					}
				} else {
					WriteOut("%-8s %-3s   %-16s %02d-%02d-%04d %2d:%02d\n",name,ext,"<DIR>",day,month,year,hour,minute);
				}
				dir_count++;
			} else {
				if (optW) {
					WriteOut("%-16s",name);
				} else {
					FormatNumber(size,numformat);
					WriteOut("%-8s %-3s   %16s %02d-%02d-%04d %2d:%02d\n",name,ext,numformat,day,month,year,hour,minute);
				}
				file_count++;
				byte_count+=size;
			}
			if (optW) {
				w_count++;
			}
		}
		if (optP && !(++p_count%(22*w_size))) {
			CMD_PAUSE(empty_string);
		}
	} while ( (ret=DOS_FindNext()) );
	if (optW) {
		if (w_count%5)	WriteOut("\n");
	}
	if (!optB) {
		/* Show the summary of results */
		FormatNumber(byte_count,numformat);
		WriteOut(MSG_Get("SHELL_CMD_DIR_BYTES_USED"),file_count,numformat);
		Bit8u drive=dta.GetSearchDrive();
		//TODO Free Space
		Bitu free_space=1024*1024*100;
		if (Drives[drive]) {
			Bit16u bytes_sector;Bit8u sectors_cluster;Bit16u total_clusters;Bit16u free_clusters;
			Drives[drive]->AllocationInfo(&bytes_sector,&sectors_cluster,&total_clusters,&free_clusters);
			free_space=bytes_sector*sectors_cluster*free_clusters;
		}
		FormatNumber(free_space,numformat);
		WriteOut(MSG_Get("SHELL_CMD_DIR_BYTES_FREE"),dir_count,numformat);
	}
	dos.dta(save_dta);
}

struct copysource {
	std::string filename;
	bool concat;
	copysource(std::string filein,bool concatin):
		filename(filein),concat(concatin){ };
	copysource():filename(""),concat(false){ };
};


void DOS_Shell::CMD_COPY(char * args) {
	HELP("COPY");
	static char defaulttarget[] = ".";
	StripSpaces(args);
	/* Command uses dta so set it to our internal dta */
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	Bit32u size;Bit16u date;Bit16u time;Bit8u attr;
	char name[DOS_NAMELENGTH_ASCII];
	std::vector<copysource> sources;
	// ignore /b and /t switches: always copy binary
	while(ScanCMDBool(args,"B")) ;
	while(ScanCMDBool(args,"T")) ; //Shouldn't this be A ?
	while(ScanCMDBool(args,"A")) ;
	ScanCMDBool(args,"Y");
	ScanCMDBool(args,"-Y");

	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		dos.dta(save_dta);
		return;
	}
	// Gather all sources (extension to copy more then 1 file specified at commandline)
	// Concatating files go as follows: All parts except for the last bear the concat flag.
	// This construction allows them to be counted (only the non concat set)
	char* source_p = NULL;
	char source_x[DOS_PATHLENGTH+CROSS_LEN];
	while ( (source_p = StripWord(args)) && *source_p ) {
		do {
			char* plus = strchr(source_p,'+');
			if (plus) *plus++ = 0;
			safe_strncpy(source_x,source_p,CROSS_LEN);
			bool has_drive_spec = false;
			size_t source_x_len = strlen(source_x);
			if (source_x_len>0) {
				if (source_x[source_x_len-1]==':') has_drive_spec = true;
			}
			if (!has_drive_spec) {
				if (DOS_FindFirst(source_p,0xffff & ~DOS_ATTR_VOLUME)) {
					dta.GetResult(name,size,date,time,attr);
					if (attr & DOS_ATTR_DIRECTORY && !strstr(source_p,"*.*"))
						strcat(source_x,"\\*.*");
				}
			}
			sources.push_back(copysource(source_x,(plus)?true:false));
			source_p = plus;
		} while(source_p && *source_p);
	}
	// At least one source has to be there
	if (!sources.size() || !sources[0].filename.size()) {
		WriteOut(MSG_Get("SHELL_MISSING_PARAMETER"));
		dos.dta(save_dta);
		return;
	};

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
		char pathSource[DOS_PATHLENGTH];
		char pathTarget[DOS_PATHLENGTH];

		if (!DOS_Canonicalize(const_cast<char*>(source.filename.c_str()),pathSource)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			dos.dta(save_dta);
			return;
		}
		// cut search pattern
		char* pos = strrchr(pathSource,'\\');
		if (pos) *(pos+1) = 0;

		if (!DOS_Canonicalize(const_cast<char*>(target.filename.c_str()),pathTarget)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			dos.dta(save_dta);
			return;
		}
		char* temp = strstr(pathTarget,"*.*");
		if(temp) *temp = 0;//strip off *.* from target
	
		// add '\\' if target is a directoy
		if (pathTarget[strlen(pathTarget)-1]!='\\') {
			if (DOS_FindFirst(pathTarget,0xffff & ~DOS_ATTR_VOLUME)) {
				dta.GetResult(name,size,date,time,attr);
				if (attr & DOS_ATTR_DIRECTORY)	
					strcat(pathTarget,"\\");
			}
		};

		//Find first sourcefile
		bool ret = DOS_FindFirst(const_cast<char*>(source.filename.c_str()),0xffff & ~DOS_ATTR_VOLUME);
		if (!ret) {
			WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),const_cast<char*>(source.filename.c_str()));
			dos.dta(save_dta);
			return;
		}

		Bit16u sourceHandle,targetHandle;
		char nameTarget[DOS_PATHLENGTH];
		char nameSource[DOS_PATHLENGTH];

		while (ret) {
			dta.GetResult(name,size,date,time,attr);

			if ((attr & DOS_ATTR_DIRECTORY)==0) {
				strcpy(nameSource,pathSource);
				strcat(nameSource,name);
				// Open Source
				if (DOS_OpenFile(nameSource,0,&sourceHandle)) {
					// Create Target or open it if in concat mode
					strcpy(nameTarget,pathTarget);
					if (nameTarget[strlen(nameTarget)-1]=='\\') strcat(nameTarget,name);

					//Don't create a newfile when in concat mode
					if (oldsource.concat || DOS_CreateFile(nameTarget,0,&targetHandle)) {
						Bit32u dummy=0;
						//In concat mode. Open the target and seek to the eof
						if (!oldsource.concat || (DOS_OpenFile(nameTarget,OPEN_READWRITE,&targetHandle) && 
					        	                  DOS_SeekFile(targetHandle,&dummy,DOS_SEEK_END))) {
							// Copy 
							static Bit8u buffer[0x8000]; // static, otherwise stack overflow possible.
							bool	failed = false;
							Bit16u	toread = 0x8000;
							do {
								failed |= DOS_ReadFile(sourceHandle,buffer,&toread);
								failed |= DOS_WriteFile(targetHandle,buffer,&toread);
							} while (toread==0x8000);
							failed |= DOS_CloseFile(sourceHandle);
							failed |= DOS_CloseFile(targetHandle);
							WriteOut(" %s\n",name);
							if(!source.concat) count++; //Only count concat files once
						} else {
							DOS_CloseFile(sourceHandle);
							WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),const_cast<char*>(target.filename.c_str()));
						}
					} else {
						DOS_CloseFile(sourceHandle);
						WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),const_cast<char*>(target.filename.c_str()));
					}
				} else WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),const_cast<char*>(source.filename.c_str()));
			};
			//On the next file
			ret = DOS_FindNext();
		};
	}

	WriteOut(MSG_Get("SHELL_CMD_COPY_SUCCESS"),count);
	dos.dta(save_dta);
}

void DOS_Shell::CMD_SET(char * args) {
	HELP("SET");
	StripSpaces(args);
	std::string line;
	if (!*args) {
		/* No command line show all environment lines */	
		Bitu count=GetEnvCount();
		for (Bitu a=0;a<count;a++) {
			if (GetEnvNum(a,line)) WriteOut("%s\n",line.c_str());			
		}
		return;
	}
	char * p=strpbrk(args, "=");
	if (!p) {
		if (!GetEnvStr(args,line)) WriteOut(MSG_Get("SHELL_CMD_SET_NOT_SET"),args);
		WriteOut("%s\n",line.c_str());
	} else {
		*p++=0;
		/* parse p for envirionment variables */
		char parsed[CMD_MAXLINE];
		char* p_parsed = parsed;
		while(*p) {
			if(*p != '%') *p_parsed++ = *p++; //Just add it (most likely path)
			else if( *(p+1) == '%') {
				*p_parsed++ = '%'; p += 2; //%% => % 
			} else {
				char * second = strchr(++p,'%');
				if(!second) continue; *second++ = 0;
				std::string temp;
				if (GetEnvStr(p,temp)) {
					std::string::size_type equals = temp.find('=');
					if (equals == std::string::npos) continue;
					strcpy(p_parsed,temp.substr(equals+1).c_str());
					p_parsed += strlen(p_parsed);
				}
				p = second;
			}
		}
		*p_parsed = 0;
		/* Try setting the variable */
		if (!SetEnv(args,parsed)) {
			WriteOut(MSG_Get("SHELL_CMD_SET_OUT_OF_SPACE"));
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
		char* word = StripWord(args);
		if (!*word) {
			WriteOut(MSG_Get("SHELL_CMD_IF_EXIST_MISSING_FILENAME"));
			return;
		}

		{	/* DOS_FindFirst uses dta so set it to our internal dta */
			RealPt save_dta=dos.dta();
			dos.dta(dos.tables.tempdta);
			bool ret=DOS_FindFirst(word,0xffff & ~DOS_ATTR_VOLUME);
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
	if (*args &&(*args==':')) args++;
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
	StripSpaces(args);
	if (!*args) {
		WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
		return;
	}
	Bit16u handle;
	char * word;
nextfile:
	word=StripWord(args);
	if (!DOS_OpenFile(word,0,&handle)) {
		WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),word);
		return;
	}
	Bit16u n;Bit8u c;
	do {
		n=1;
		DOS_ReadFile(handle,&c,&n);
		DOS_WriteFile(STDOUT,&c,&n);
	} while (n);
	DOS_CloseFile(handle);
	if (*args) goto nextfile;
}

void DOS_Shell::CMD_REM(char * args) {
	HELP("REM");
}

void DOS_Shell::CMD_PAUSE(char * args){
	HELP("PAUSE");
	WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
	Bit8u c;Bit16u n=1;
	DOS_ReadFile (STDIN,&c,&n);
}

void DOS_Shell::CMD_CALL(char * args){
	HELP("CALL");
	this->call=true; /* else the old batchfile will be closed first */
	this->ParseLine(args);
	this->call=false;
}

void DOS_Shell::CMD_SUBST (char * args) {
/* If more that one type can be substed think of something else 
 * E.g. make basedir member dos_drive instead of localdrive
 */
	HELP("SUBST");
	localDrive* ldp=0;
	char mountstring[DOS_PATHLENGTH+CROSS_LEN+20];
	char temp_str[2] = { 0,0 };
	try {
		strcpy(mountstring,"MOUNT ");
		StripSpaces(args);
		std::string arg;
		CommandLine command(0,args);

		if (command.GetCount() != 2) throw 0 ;
		command.FindCommand(2,arg);
		if((arg=="/D" ) || (arg=="/d")) throw 1; //No removal (one day)
  
		command.FindCommand(1,arg);
		if( (arg.size()>1) && arg[1] !=':')  throw(0);
		temp_str[0]=(char)toupper(args[0]);
		if(Drives[temp_str[0]-'A'] ) throw 0; //targetdrive in use
		strcat(mountstring,temp_str);
		strcat(mountstring," ");

		command.FindCommand(2,arg);
   		Bit8u drive;char fulldir[DOS_PATHLENGTH];
		if (!DOS_MakeName(const_cast<char*>(arg.c_str()),fulldir,&drive)) throw 0;
	
		if( ( ldp=dynamic_cast<localDrive*>(Drives[drive])) == 0 ) throw 0;
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
		if(a == 0) {
			WriteOut(MSG_Get("SHELL_CMD_SUBST_FAILURE"));
		} else {
		       	WriteOut(MSG_Get("SHELL_CMD_SUBST_NO_REMOVE"));
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
	if(!optS) while ((c = *ptr)) *ptr++ = (char)toupper(c); /* When in no case-sensitive mode. make everything upcase */
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
	} while (!c || !(ptr = strchr(rem,(optS?c:toupper(c)))));
	c = optS?c:(Bit8u)toupper(c);
	DOS_WriteFile (STDOUT,&c, &n);
	dos.return_code = (Bit8u)(ptr-rem+1);
}

void DOS_Shell::CMD_ATTRIB(char *args){
	HELP("ATTRIB");
	// No-Op for now.
}

void DOS_Shell::CMD_PATH(char *args){
	HELP("PATH");
	if(args && *args && strlen(args)){
		char pathstring[DOS_PATHLENGTH+CROSS_LEN+20]={ 0 };
		strcpy(pathstring,"set PATH=");
		while(args && *args && (*args=='='|| *args==' ')) 
		     args++;
		strcat(pathstring,args);
		this->ParseLine(pathstring);
		return;
	} else {
		std::string line;
		if(GetEnvStr("PATH",line)) {
        		WriteOut("%s",line.c_str());
		} else {
			WriteOut("PATH=(null)");
		}
	}
}

void DOS_Shell::CMD_VER(char *args) {
	HELP("VER");
	if(args && *args) {
		char* word = StripWord(args);
		if(strcasecmp(word,"set")) return;
		word = StripWord(args);
		dos.version.major = (Bit8u)(atoi(word));
		dos.version.minor = (Bit8u)(atoi(args));
	} else WriteOut(MSG_Get("SHELL_CMD_VER_VER"),VERSION,dos.version.major,dos.version.minor);
}
