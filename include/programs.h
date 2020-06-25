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
	enum opt_style {
		dos=0,		// MS-DOS style /switches
		gnu,		// GNU style --switches or -switches, switch parsing stops at --
		gnu_getopt,	// GNU style --long or -a -b -c -d or -abcd (short as single char), switch parsing stops at --
		either		// both dos and gnu, switch parsing stops at --
	};
public:
	CommandLine(int argc,char const * const argv[],enum opt_style opt=CommandLine::either);
	CommandLine(char const * const name,char const * const cmdline,enum opt_style opt=CommandLine::either);
	const char * GetFileName(){ return file_name.c_str();}

	bool FindExist(char const * const name,bool remove=false);
	bool FindHex(char const * const name,int & value,bool remove=false);
	bool FindInt(char const * const name,int & value,bool remove=false);
	bool FindString(char const * const name,std::string & value,bool remove=false);
	bool FindCommand(unsigned int which,std::string & value);
	bool FindStringBegin(char const * const begin,std::string & value, bool remove=false);
	bool FindStringRemain(char const * const name,std::string & value);
	bool FindStringRemainBegin(char const * const name,std::string & value);
	bool GetStringRemain(std::string & value);
	int GetParameterFromList(const char* const params[], std::vector<std::string> & output);
	void FillVector(std::vector<std::string> & vector);
	unsigned int GetCount(void);
	void Shift(unsigned int amount=1);
	Bit16u Get_arglength();

	bool BeginOpt(bool eat_argv=true);
	bool GetOpt(std::string &name);
	bool NextOptArgv(std::string &argv);
	bool GetOptGNUSingleCharCheck(std::string &name);
	void ChangeOptStyle(enum opt_style opt_style);
	void EndOpt();

    bool GetCurrentArgv(std::string &argv);
    bool CurrentArgvEnd(void);
    void EatCurrentArgv(void);
    void NextArgv(void);

    const std::string &GetRawCmdline(void);
private:
	typedef std::list<std::string>::iterator cmd_it;
	std::string opt_gnu_getopt_singlechar;		/* non-empty if we hit GNU options like -abcd => -a -b -c -d */
	cmd_it opt_scan;
	bool opt_eat_argv = false;
	std::list<std::string> cmds;
	std::string file_name;
    std::string raw_cmdline;
	enum opt_style opt_style;
	bool FindEntry(char const * const name,cmd_it & it,bool neednext=false);
};

/*! \brief          Base Program class for built-in programs on drive Z:
 *
 *  \description    This provides the base class for built-in programs registered on drive Z:.
 *                  In most cases, the class will override just the Run() method.
 */
class Program {
public:
	Program();                                          //! Constructor
	virtual ~Program(){                                 //! Default destructor
		if (cmd != NULL) delete cmd;
		if (psp != NULL) delete psp;
	}

    /*! \brief      Exit status of the program
     */
	unsigned char exit_status;                          //! Exit status, returned to the parent DOS process

	std::string temp_line;                              //! Temporary string object for parsing
	CommandLine * cmd;                                  //! Command line object
	DOS_PSP * psp;                                      //! DOS kernel Program Segment Prefix associated with this program at runtime
	virtual void Run(void)=0;                           //! Run() method, called when the program is run. Subclass must override this
	bool GetEnvStr(const char * entry,std::string & result); //! Return an environment variable by name
	bool GetEnvNum(Bitu want_num,std::string & result);      //! Return an environment variable by index
	Bitu GetEnvCount(void);                             //! Return the number of enviormental variables
	bool SetEnv(const char * entry,const char * new_string); //! Set environment variable
	void WriteOut(const char * format,...);				//! Write to standard output 
	void WriteOut_NoParsing(const char * format);		//! Write to standard output, no parsing
	void ChangeToLongCmd();                             //! Get command line from shell instead of PSP
	void DebugDumpEnv();                                //! Dump environment block to log
	void WriteExitStatus();                             //! Write exit status to CPU register AL for return to MS-DOS
};

typedef void (PROGRAMS_Main)(Program * * make);
void PROGRAMS_MakeFile(char const * const name,PROGRAMS_Main * SDL_main);

#endif
