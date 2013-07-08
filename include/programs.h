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

/* $Id: programs.h,v 1.19 2009-05-27 09:15:41 qbix79 Exp $ */

#ifndef DOSBOX_PROGRAMS_H
#define DOSBOX_PROGRAMS_H

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif
#ifndef DOSBOX_DOS_INC_H
#include "dos_inc.h"
#endif

#ifndef CH_LIST
#define CH_LIST
#include <list>
#endif

#ifndef CH_STRING
#define CH_STRING
#include <string>
#endif

class CommandLine {
public:
	CommandLine(int argc,char const * const argv[]);
	CommandLine(char const * const name,char const * const cmdline);
	const char * GetFileName(){ return file_name.c_str();}

	bool FindExist(char const * const name,bool remove=false);
	bool FindHex(char const * const name,int & value,bool remove=false);
	bool FindInt(char const * const name,int & value,bool remove=false);
	bool FindString(char const * const name,std::string & value,bool remove=false);
	bool FindCommand(unsigned int which,std::string & value);
	bool FindStringBegin(char const * const begin,std::string & value, bool remove=false);
	bool FindStringRemain(char const * const name,std::string & value);
	bool GetStringRemain(std::string & value);
	unsigned int GetCount(void);
	void Shift(unsigned int amount=1);
	Bit16u Get_arglength();

private:
	typedef std::list<std::string>::iterator cmd_it;
	std::list<std::string> cmds;
	std::string file_name;
	bool FindEntry(char const * const name,cmd_it & it,bool neednext=false);
};

class Program {
public:
	Program();
	virtual ~Program(){
		delete cmd;
		delete psp;
	}
	std::string temp_line;
	CommandLine * cmd;
	DOS_PSP * psp;
	virtual void Run(void)=0;
	bool GetEnvStr(const char * entry,std::string & result);
	bool GetEnvNum(Bitu num,std::string & result);
	Bitu GetEnvCount(void);
	bool SetEnv(const char * entry,const char * new_string);
	void WriteOut(const char * format,...);				/* Write to standard output */
	void WriteOut_NoParsing(const char * format);				/* Write to standard output, no parsing */
	void ChangeToLongCmd();

};

typedef void (PROGRAMS_Main)(Program * * make);
void PROGRAMS_MakeFile(char const * const name,PROGRAMS_Main * main);

#endif
