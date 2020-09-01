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


#ifndef DOSBOX_SHELL_H
#define DOSBOX_SHELL_H

#include <ctype.h>
#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif
#ifndef DOSBOX_PROGRAMS_H
#include "programs.h"
#endif

#include <string>
#include <list>
#include <map>

#include <SDL.h>
#if SDL_VERSION_ATLEAST(2, 0, 0)
#define SDL_STRING "SDL2"
#else
#define SDL_STRING "SDL1"
#endif

#define CMD_MAXLINE 4096
#define CMD_MAXCMDS 20
#define CMD_OLDSIZE 4096
extern Bitu call_shellstop;
class DOS_Shell;

/* first_shell is used to add and delete stuff from the shell env 
 * by "external" programs. (config) */
extern DOS_Shell * first_shell;


class BatchFile {
public:
	BatchFile(DOS_Shell * host,char const* const resolved_name,char const* const entered_name, char const * const cmd_line);
	virtual ~BatchFile();
	virtual bool ReadLine(char * line);
	bool Goto(const char * where);
	void Shift(void);
	Bit16u file_handle;
	Bit32u location;
	bool echo;
	DOS_Shell * shell;
	BatchFile * prev;
	CommandLine * cmd;
	std::string filename;
};

class AutoexecEditor;

/*! \brief          DOS shell program object
 *
 *  \description    This is the DOS shell, including built-in commands
 */
class DOS_Shell : public Program {
private:
	friend class AutoexecEditor;
	std::list<std::string> l_history, l_completion;
    template<class _Type>
    struct less_ignore_case {
        typedef _Type first_argument_type;
        typedef _Type second_argument_type;
        typedef bool result_type;

        constexpr bool operator()(const _Type& _Left, const _Type& _Right) const {
            return strcasecmp(_Left.c_str(), _Right.c_str()) < 0;
        }
    };
    typedef std::map<std::string, std::string, less_ignore_case<std::string> > cmd_alias_map_t;
    cmd_alias_map_t cmd_alias;

	Bit16u completion_index;
	
private:
	void ProcessCmdLineEnvVarStitution(char * line);

public:

	DOS_Shell();
	virtual ~DOS_Shell();

    /*! \brief      Program entry point, when the command is run
     */
	void Run(void);

    /*! \brief      Alternate execution if /C switch is given
     */
	void RunInternal(void); //for command /C

/* A load of subfunctions */

    /*! \brief      Line parsing function
     */
	void ParseLine(char * line);

    /*! \brief      Redirection handling
     */
	Bitu GetRedirection(char *s, char **ifn, char **ofn, char **toc,bool * append);

    /*! \brief      Command line input and keyboard handling
     */
	void InputCommand(char * line);

    /*! \brief      Render and output command prompt
     */
	void ShowPrompt();

    /*! \brief      Process and execute command (internal or external)
     */
	void DoCommand(char * line);

    /*! \brief      Execute a command
     */
    bool Execute(char* name, const char* args);

	/*! \brief      Checks if it matches a hardware-property */
	bool CheckConfig(char* cmd_in,char*line);

    /*! \brief      Given a command, look up the path using default paths and the PATH variable
     */
	char * Which(char * name);

    /*! \brief      Online HELP for the shell
     */
	void CMD_HELP(char * args);

    /*! \brief      Exteneded Ctrl+C switch
     */
	void CMD_BREAK(char * args);

    /*! \brief      Clear screen (CLS)
     */
	void CMD_CLS(char * args);

    /*! \brief      File copy command
     */
	void CMD_COPY(char * args);

    /*! \brief      Command to set date (DATE)
     */
	void CMD_DATE(char * args);

    /*! \brief      Command to set time (TIME)
     */
	void CMD_TIME(char * args);

    /*! \brief      Directory listing (DIR)
     */
	void CMD_DIR(char * args);

    /*! \brief      Deletion command (DEL)
     */
	void CMD_DELETE(char * args);

    /*! \brief      Echo command (ECHO)
     */
	void CMD_ECHO(char * args);

    /*! \brief      Exit command (EXIT)
     */
	void CMD_EXIT(char * args);

    /*! \brief      Directory creation (MKDIR)
     */
	void CMD_MKDIR(char * args);

    /*! \brief      Change current directory (CD)
     */
	void CMD_CHDIR(char * args);

    /*! \brief      Directory deletion (RMDIR)
     */
	void CMD_RMDIR(char * args);

    /*! \brief      Environment variable setting/management (SET)
     */
	void CMD_SET(char * args);

    /*! \brief      Conditional execution (IF)
     */
	void CMD_IF(char * args);

    /*! \brief      Batch file branching (GOTO)
     */
	void CMD_GOTO(char * args);

    /*! \brief      Print file to console (TYPE)
     */
	void CMD_TYPE(char * args);

    /*! \brief      Human readable comment (REM)
     */
	void CMD_REM(char * args);

    /*! \brief      File rename (REN)
     */
	void CMD_RENAME(char * args);

    /*! \brief      Execute batch file as sub-program (CALL)
     */
	void CMD_CALL(char * args);

    /*! \brief      Print generic Syntax Error message to console
     */
	void SyntaxError(void);

    /*! \brief      Pause and wait for user to hit Enter (PAUSE)
     */
	void CMD_PAUSE(char * args);

    /*! \brief      Map drive letter to folder (SUBST)
     */
	void CMD_SUBST(char* args);

    /*! \brief      Load a program into high memory if possible
     */
	void CMD_LOADHIGH(char* args);

    /*! \brief      Prompt for a choice (CHOICE)
     */
	void CMD_CHOICE(char * args);

    /*! \brief      Set file attributes (ATTRIB)
     */
	void CMD_ATTRIB(char * args);

    /*! \brief      Set PATH variable (PATH)
     */
	void CMD_PATH(char * args);

    /*! \brief      Consume one command line argument (SHIFT)
     */
	void CMD_SHIFT(char * args);

    /*! \brief      File verification switch
     */
	void CMD_VERIFY(char * args);

    /*! \brief      Print DOS version (VER)
     */
	void CMD_VER(char * args);

    /*! \brief      TODO?
     */
	void CMD_ADDKEY(char * args);

    /*! \brief      TODO?
     */
	void CMD_VOL(char * args);

    /*! \brief      Change DOS prompt pattern (PROMPT)
     */
	void CMD_PROMPT(char * args);

    /*! \brief      Text pager (MORE)
     */
	void CMD_MORE(char * args);

    /*! \brief      Change TTY (console) device (CTTY)
     */
	void CMD_CTTY(char * args);

    /*! \brief      Change country code
     */
	void CMD_COUNTRY(char * args);
    void CMD_TRUENAME(char * args);
    void CMD_DXCAPTURE(char * args);

    /*! \brief      Looping execution (FOR)
     */
	void CMD_FOR(char * args);

    /*! \brief      LFN switch for FOR
     */
	void CMD_LFNFOR(char * args);

    /*! \brief      ALIAS
    */
	void CMD_ALIAS(char* args);

    /*! \brief      LS
    */
	void CMD_LS(char *args);

#if C_DEBUG
    /*! \brief      Execute command within debugger (break at entry point)
     */
	void CMD_DEBUGBOX(char * args);

    /*! \brief      INT 2Fh debugging tool
     */
	void CMD_INT2FDBG(char * args);
#endif

	/* The shell's variables */
	Bit16u input_handle;
	BatchFile * bf;                     //! Batch file to execute
	bool echo;
	bool exit;
	bool call;
    bool exec;
    bool perm;
	bool lfnfor;
    /* Status */
    bool input_eof;                     //! STDIN has hit EOF
};

struct SHELL_Cmd {
	const char * name;								/* Command name*/
	Bit32u flags;									/* Flags about the command */
	void (DOS_Shell::*handler)(char * args);		/* Handler for this command */
	const char * help;								/* String with command help */
};

/* Object to manage lines in the autoexec.bat The lines get removed from
 * the file if the object gets destroyed. The environment is updated
 * as well if the line set a a variable */
class AutoexecObject{
private:
	bool installed;
	std::string buf;
public:
	AutoexecObject():installed(false){ };
	void Install(std::string const &in);
	void InstallBefore(std::string const &in);
	void Uninstall();
	~AutoexecObject();
private:
	void CreateAutoexec(void);
};

#endif
