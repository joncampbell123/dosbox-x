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

/* $Id: programs.cpp,v 1.37 2009-05-27 09:15:42 qbix79 Exp $ */

#include <vector>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "programs.h"
#include "callback.h"
#include "regs.h"
#include "support.h"
#include "cross.h"
#include "control.h"
#include "shell.h"

Bitu call_program;

/* This registers a file on the virtual drive and creates the correct structure for it*/

static Bit8u exe_block[]={
	0xbc,0x00,0x04,					//MOV SP,0x400 decrease stack size
	0xbb,0x40,0x00,					//MOV BX,0x040 for memory resize
	0xb4,0x4a,						//MOV AH,0x4A	Resize memory block
	0xcd,0x21,						//INT 0x21
//pos 12 is callback number
	0xFE,0x38,0x00,0x00,			//CALLBack number
	0xb8,0x00,0x4c,					//Mov ax,4c00
	0xcd,0x21,						//INT 0x21
};

#define CB_POS 12

static std::vector<PROGRAMS_Main*> internal_progs;

void PROGRAMS_MakeFile(char const * const name,PROGRAMS_Main * main) {
	Bit8u * comdata=(Bit8u *)malloc(32); //MEM LEAK
	memcpy(comdata,&exe_block,sizeof(exe_block));
	comdata[CB_POS]=(Bit8u)(call_program&0xff);
	comdata[CB_POS+1]=(Bit8u)((call_program>>8)&0xff);

	/* Copy save the pointer in the vector and save it's index */
	if (internal_progs.size()>255) E_Exit("PROGRAMS_MakeFile program size too large (%d)",static_cast<int>(internal_progs.size()));
	Bit8u index = (Bit8u)internal_progs.size();
	internal_progs.push_back(main);

	memcpy(&comdata[sizeof(exe_block)],&index,sizeof(index));
	Bit32u size=sizeof(exe_block)+sizeof(index);	
	VFILE_Register(name,comdata,size);	
}



static Bitu PROGRAMS_Handler(void) {
	/* This sets up everything for a program start up call */
	Bitu size=sizeof(Bit8u);
	Bit8u index;
	/* Read the index from program code in memory */
	PhysPt reader=PhysMake(dos.psp(),256+sizeof(exe_block));
	HostPt writer=(HostPt)&index;
	for (;size>0;size--) *writer++=mem_readb(reader++);
	Program * new_program;
	if(index > internal_progs.size()) E_Exit("something is messing with the memory");
	PROGRAMS_Main * handler = internal_progs[index];
	(*handler)(&new_program);
	new_program->Run();
	delete new_program;
	return CBRET_NONE;
}


/* Main functions used in all program */


Program::Program() {
	/* Find the command line and setup the PSP */
	psp = new DOS_PSP(dos.psp());
	/* Scan environment for filename */
	PhysPt envscan=PhysMake(psp->GetEnvironment(),0);
	while (mem_readb(envscan)) envscan+=mem_strlen(envscan)+1;	
	envscan+=3;
	CommandTail tail;
	MEM_BlockRead(PhysMake(dos.psp(),128),&tail,128);
	if (tail.count<127) tail.buffer[tail.count]=0;
	else tail.buffer[126]=0;
	char filename[256+1];
	MEM_StrCopy(envscan,filename,256);
	cmd = new CommandLine(filename,tail.buffer);
}

extern std::string full_arguments;

void Program::ChangeToLongCmd() {
	/* 
	 * Get arguments directly from the shell instead of the psp.
	 * this is done in securemode: (as then the arguments to mount and friends
	 * can only be given on the shell ( so no int 21 4b) 
	 * Securemode part is disabled as each of the internal command has already
	 * protection for it. (and it breaks games like cdman)
	 * it is also done for long arguments to as it is convient (as the total commandline can be longer then 127 characters.
	 * imgmount with lot's of parameters
	 * Length of arguments can be ~120. but switch when above 100 to be sure
	 */

	if(/*control->SecureMode() ||*/ cmd->Get_arglength() > 100) {	
		CommandLine* temp = new CommandLine(cmd->GetFileName(),full_arguments.c_str());
		delete cmd;
		cmd = temp;
	}
	full_arguments.assign(""); //Clear so it gets even more save
}

void Program::WriteOut(const char * format,...) {
	char buf[2048];
	va_list msg;
	
	va_start(msg,format);
	vsnprintf(buf,2047,format,msg);
	va_end(msg);

	Bit16u size = (Bit16u)strlen(buf);
	for(Bit16u i = 0; i < size;i++) {
		Bit8u out;Bit16u s=1;
		if(buf[i] == 0xA && i > 0 && buf[i-1] !=0xD) {
			out = 0xD;DOS_WriteFile(STDOUT,&out,&s);
		}
		out = buf[i];
		DOS_WriteFile(STDOUT,&out,&s);
	}
	
//	DOS_WriteFile(STDOUT,(Bit8u *)buf,&size);
}

void Program::WriteOut_NoParsing(const char * format) {
	Bit16u size = (Bit16u)strlen(format);
	char const* buf = format;
	for(Bit16u i = 0; i < size;i++) {
		Bit8u out;Bit16u s=1;
		if(buf[i] == 0xA && i > 0 && buf[i-1] !=0xD) {
			out = 0xD;DOS_WriteFile(STDOUT,&out,&s);
		}
		out = buf[i];
		DOS_WriteFile(STDOUT,&out,&s);
	}

//	DOS_WriteFile(STDOUT,(Bit8u *)format,&size);
}


bool Program::GetEnvStr(const char * entry,std::string & result) {
	/* Walk through the internal environment and see for a match */
	PhysPt env_read=PhysMake(psp->GetEnvironment(),0);

	char env_string[1024+1];
	result.erase();
	if (!entry[0]) return false;
	do 	{
		MEM_StrCopy(env_read,env_string,1024);
		if (!env_string[0]) return false;
		env_read += (PhysPt)(strlen(env_string)+1);
		char* equal = strchr(env_string,'=');
		if (!equal) continue;
		/* replace the = with \0 to get the length */
		*equal = 0;
		if (strlen(env_string) != strlen(entry)) continue;
		if (strcasecmp(entry,env_string)!=0) continue;
		/* restore the = to get the original result */
		*equal = '=';
		result = env_string;
		return true;
	} while (1);
	return false;
}

bool Program::GetEnvNum(Bitu num,std::string & result) {
	char env_string[1024+1];
	PhysPt env_read=PhysMake(psp->GetEnvironment(),0);
	do 	{
		MEM_StrCopy(env_read,env_string,1024);
		if (!env_string[0]) break;
		if (!num) { result=env_string;return true;}
		env_read += (PhysPt)(strlen(env_string)+1);
		num--;
	} while (1);
	return false;
}

Bitu Program::GetEnvCount(void) {
	PhysPt env_read=PhysMake(psp->GetEnvironment(),0);
	Bitu num=0;
	while (mem_readb(env_read)!=0) {
		for (;mem_readb(env_read);env_read++) {};
		env_read++;
		num++;
	}
	return num;
}

bool Program::SetEnv(const char * entry,const char * new_string) {
	PhysPt env_read=PhysMake(psp->GetEnvironment(),0);
	PhysPt env_write=env_read;
	char env_string[1024+1];
	do 	{
		MEM_StrCopy(env_read,env_string,1024);
		if (!env_string[0]) break;
		env_read += (PhysPt)(strlen(env_string)+1);
		if (!strchr(env_string,'=')) continue;		/* Remove corrupt entry? */
		if ((strncasecmp(entry,env_string,strlen(entry))==0) && 
			env_string[strlen(entry)]=='=') continue;
		MEM_BlockWrite(env_write,env_string,(Bitu)(strlen(env_string)+1));
		env_write += (PhysPt)(strlen(env_string)+1);
	} while (1);
/* TODO Maybe save the program name sometime. not really needed though */
	/* Save the new entry */
	if (new_string[0]) {
		std::string bigentry(entry);
		for (std::string::iterator it = bigentry.begin(); it != bigentry.end(); ++it) *it = toupper(*it);
		sprintf(env_string,"%s=%s",bigentry.c_str(),new_string); 
//		sprintf(env_string,"%s=%s",entry,new_string); //oldcode
		MEM_BlockWrite(env_write,env_string,(Bitu)(strlen(env_string)+1));
		env_write += (PhysPt)(strlen(env_string)+1);
	}
	/* Clear out the final piece of the environment */
	mem_writed(env_write,0);
	return true;
}

class CONFIG : public Program {
public:
	void Run(void);
};

void MSG_Write(const char *);

void CONFIG::Run(void) {
	FILE * f;
	if (cmd->FindString("-writeconf",temp_line,true) 
			|| cmd->FindString("-wc",temp_line,true)) {
		/* In secure mode don't allow a new configfile to be created */
		if(control->SecureMode()) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
			return;
		}
		f=fopen(temp_line.c_str(),"wb+");
		if (!f) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_FILE_ERROR"),temp_line.c_str());
			return;
		}
		fclose(f);
		control->PrintConfig(temp_line.c_str());
		return;
	}
	if (cmd->FindString("-writelang",temp_line,true)
			||cmd->FindString("-wl",temp_line,true)) {
		/* In secure mode don't allow a new languagefile to be created
		 * Who knows which kind of file we would overwriting. */
		if(control->SecureMode()) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
			return;
		}
		f=fopen(temp_line.c_str(),"wb+");
		if (!f) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_FILE_ERROR"),temp_line.c_str());
			return;
		}
		fclose(f);
		MSG_Write(temp_line.c_str());
		return;
	}

	/* Code for switching to secure mode */
	if(cmd->FindExist("-securemode",true)) {
		control->SwitchToSecureMode();
		WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_ON"));
		return;
	}
   
	/* Code for getting the current configuration.           *
	 * Official format: config -get "section property"       *
	 * As a bonus it will set %CONFIG% to this value as well */
	if(cmd->FindString("-get",temp_line,true)) {
		std::string temp2 = "";
		cmd->GetStringRemain(temp2);//So -get n1 n2= can be used without quotes
		if(temp2 != "") temp_line = temp_line + " " + temp2;

		std::string::size_type space = temp_line.find(" ");
		if(space == std::string::npos) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_GET_SYNTAX"));
			return;
		}
		//Copy the found property to a new string and erase from templine (mind the space)
		std::string prop = temp_line.substr(space+1); temp_line.erase(space);

		Section* sec = control->GetSection(temp_line.c_str());
		if(!sec) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_ERROR"),temp_line.c_str());
			return;
		}
		std::string val = sec->GetPropValue(prop.c_str());
		if(val == NO_SUCH_PROPERTY) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_NO_PROPERTY"),prop.c_str(),temp_line.c_str());   
			return;
		}
		WriteOut("%s",val.c_str());
		first_shell->SetEnv("CONFIG",val.c_str());
		return;
	}



	/* Code for the configuration changes                                  *
	 * Official format: config -set "section property=value"               *
	 * Accepted: without quotes and/or without -set and/or without section *
	 *           and/or the "=" replaced by a " "                          */

	if (cmd->FindString("-set",temp_line,true)) { //get all arguments
		std::string temp2 = "";
		cmd->GetStringRemain(temp2);//So -set n1 n2=n3 can be used without quotes
		if(temp2!="") temp_line = temp_line + " " + temp2;
	} else 	if(!cmd->GetStringRemain(temp_line)) {//no set 
			WriteOut(MSG_Get("PROGRAM_CONFIG_USAGE")); //and no arguments specified
			return;
	};
	//Wanted input: n1 n2=n3
	char copy[1024];
	strcpy(copy,temp_line.c_str());
	//seperate section from property
	const char* temp = strchr(copy,' ');
	if((temp && *temp) || (temp=strchr(copy,'=')) ) copy[temp++ - copy]= 0;
	else {
		WriteOut(MSG_Get("PROGRAM_CONFIG_USAGE"));
		return;
	}
	//if n1 n2 n3 then replace last space with =
	const char* sign = strchr(temp,'=');
	if(!sign) {
		sign = strchr(temp,' ');
		if(sign) {
			copy[sign - copy] = '=';
		} else {
			//2 items specified (no space nor = between n2 and n3
			//assume that they posted: property value
			//Try to determine the section.
			Section* sec=control->GetSectionFromProperty(copy);
			if(!sec){
				if(control->GetSectionFromProperty(temp)) return; //Weird situation:ignore
				WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"),copy);
				return;
			} //Hack to allow config ems true
			char buffer[1024];strcpy(buffer,copy);strcat(buffer,"=");strcat(buffer,temp);
			sign = strchr(buffer,' '); 
			if(sign) buffer[sign - buffer] = '=';
			strcpy(copy,sec->GetName());
			temp = buffer;
		}
	}
	
	/* Input processed. Now the real job starts
	 * copy contains the likely "sectionname" 
	 * temp contains "property=value" 
	 * the section is destroyed and a new input line is given to
	 * the configuration parser. Then the section is restarted.
	 */
	char* inputline = const_cast<char*>(temp);
	Section* sec = 0;
	sec = control->GetSection(copy);
	if(!sec) { WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_ERROR"),copy);return;}
	sec->ExecuteDestroy(false);
	sec->HandleInputline(inputline);
	sec->ExecuteInit(false);
	return;
}


static void CONFIG_ProgramStart(Program * * make) {
	*make=new CONFIG;
}


void PROGRAMS_Init(Section* /*sec*/) {
	/* Setup a special callback to start virtual programs */
	call_program=CALLBACK_Allocate();
	CALLBACK_Setup(call_program,&PROGRAMS_Handler,CB_RETF,"internal program");
	PROGRAMS_MakeFile("CONFIG.COM",CONFIG_ProgramStart);

	MSG_Add("PROGRAM_CONFIG_FILE_ERROR","Can't open file %s\n");
	MSG_Add("PROGRAM_CONFIG_USAGE","Config tool:\nUse -writeconf filename to write the current config.\nUse -writelang filename to write the current language strings.\n");
	MSG_Add("PROGRAM_CONFIG_SECURE_ON","Switched to secure mode.\n");
	MSG_Add("PROGRAM_CONFIG_SECURE_DISALLOW","This operation is not permitted in secure mode.\n");
	MSG_Add("PROGRAM_CONFIG_SECTION_ERROR","Section %s doesn't exist.\n");
	MSG_Add("PROGRAM_CONFIG_PROPERTY_ERROR","No such section or property.\n");
	MSG_Add("PROGRAM_CONFIG_NO_PROPERTY","There is no property %s in section %s.\n");
	MSG_Add("PROGRAM_CONFIG_GET_SYNTAX","Correct syntax: config -get \"section property\".\n");
}
