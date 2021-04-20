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


#include <vector>
#include <sstream>
#include <ctype.h>
#include <assert.h>
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
#include "hardware.h"
#include "mapper.h"
#include "menu.h"
#include "render.h"
#include "../ints/int10.h"
#if defined(WIN32)
#include "windows.h"
extern RECT monrect;
extern int curscreen;
typedef struct {
	int	x, y;
} xyp;
#endif

Bitu call_program;
extern const char *modifier;
extern int enablelfn, paste_speed, wheel_key, freesizecap, wpType, wpVersion, wpBG, lastset;
extern bool dos_kernel_disabled, force_nocachedir, wpcolon, lockmount, enable_config_as_shell_commands, load, winrun, winautorun, startwait, startquiet, mountwarning, wheel_guest, clipboard_dosapi, noremark_save_state, force_load_state, sync_time, manualtime, showbold, showital, showline, showsout, char512;

/* This registers a file on the virtual drive and creates the correct structure for it*/

static uint8_t exe_block[]={
	0xbc,0x00,0x04,					//0x100 MOV SP,0x400  decrease stack size
	0xbb,0x40,0x00,					//0x103 MOV BX,0x0040 for memory resize
	0xb4,0x4a,					//0x106 MOV AH,0x4A   Resize memory block
	0xcd,0x21,					//0x108 INT 0x21      ...
	0x30,0xc0,					//0x10A XOR AL,AL     Clear AL (exit code). Program will write AL to modify exit status
//pos 14 is callback number
	0xFE,0x38,0x00,0x00,				//0x10C CALLBack number
	0xb4,0x4c,					//0x110 Mov AH,0x4C   Prepare to exit, preserve AL
	0xcd,0x21					//0x112 INT 0x21      Exit to DOS
};							//0x114 --DONE--

#define CB_POS 14

class InternalProgramEntry {
public:
	InternalProgramEntry() {
		main = NULL;
		comsize = 0;
		comdata = NULL;
	}
	~InternalProgramEntry() {
		if (comdata != NULL) free(comdata);
		comdata = NULL;
		comsize = 0;
		main = NULL;
	}
public:
	std::string	name;
	uint8_t*		comdata;
	uint32_t		comsize;
	PROGRAMS_Main*	main;
};

static std::vector<InternalProgramEntry*> internal_progs;
void EMS_Startup(Section* sec), EMS_DoShutDown(), resetFontSize();

void PROGRAMS_Shutdown(void) {
	LOG(LOG_MISC,LOG_DEBUG)("Shutting down internal programs list");

	for (size_t i=0;i < internal_progs.size();i++) {
		if (internal_progs[i] != NULL) {
			delete internal_progs[i];
			internal_progs[i] = NULL;
		}
	}
	internal_progs.clear();
}

void PROGRAMS_MakeFile(char const * const name,PROGRAMS_Main * main) {
	uint32_t size=sizeof(exe_block)+sizeof(uint8_t);
	InternalProgramEntry *ipe;
	uint8_t *comdata;
	uint8_t index;

	/* Copy save the pointer in the vector and save it's index */
	if (internal_progs.size()>255) E_Exit("PROGRAMS_MakeFile program size too large (%d)",static_cast<int>(internal_progs.size()));

	index = (uint8_t)internal_progs.size();
	comdata = (uint8_t *)malloc(32); //MEM LEAK
	memcpy(comdata,&exe_block,sizeof(exe_block));
	memcpy(&comdata[sizeof(exe_block)],&index,sizeof(index));
	comdata[CB_POS]=(uint8_t)(call_program&0xff);
	comdata[CB_POS+1]=(uint8_t)((call_program>>8)&0xff);

	ipe = new InternalProgramEntry();
	ipe->main = main;
	ipe->name = name;
	ipe->comsize = size;
	ipe->comdata = comdata;
	internal_progs.push_back(ipe);
	VFILE_Register(name,ipe->comdata,ipe->comsize);
}

static Bitu PROGRAMS_Handler(void) {
	/* This sets up everything for a program start up call */
	Bitu size=sizeof(uint8_t);
	uint8_t index;
	/* Read the index from program code in memory */
	PhysPt reader=PhysMake(dos.psp(),256+sizeof(exe_block));
	HostPt writer=(HostPt)&index;
	for (;size>0;size--) *writer++=mem_readb(reader++);
	Program * new_program = NULL;
	if (index >= internal_progs.size()) E_Exit("something is messing with the memory");
	InternalProgramEntry *ipe = internal_progs[index];
	if (ipe == NULL) E_Exit("Attempt to run internal program slot with nothing allocated");
	if (ipe->main == NULL) return CBRET_NONE;
	PROGRAMS_Main * handler = internal_progs[index]->main;
	(*handler)(&new_program);

	try { /* "BOOT" can throw an exception (int(2)) */
		new_program->Run();
		delete new_program;
	}
	catch (...) { /* well if it happened, free the program anyway to avert memory leaks */
		delete new_program;
		throw; /* pass it on */
	}

	return CBRET_NONE;
}

/* Main functions used in all program */

Program::Program() {
	/* Find the command line and setup the PSP */
	psp = new DOS_PSP(dos.psp());
	/* Scan environment for filename */
	PhysPt envscan=PhysMake(psp->GetEnvironment(),0);
	while (mem_readb(envscan)) envscan+=(PhysPt)(mem_strlen(envscan)+1);	
	envscan+=3;
	CommandTail tail;
    MEM_BlockRead(PhysMake(dos.psp(),CTBUF+1),&tail,CTBUF+1);
    if (tail.count<CTBUF) tail.buffer[tail.count]=0;
    else tail.buffer[CTBUF-1]=0;
	char filename[256+1];
	MEM_StrCopy(envscan,filename,256);
	cmd = new CommandLine(filename,tail.buffer);
	exit_status = 0;
}

extern std::string full_arguments;

void Program::WriteExitStatus() {
	/* the exe block was modified that on return to DOS only AH is modified, leaving AL normally
	 * zero but we're free to set AL to any other value to set exit code */
	reg_al = exit_status;
}

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

	if (/*control->SecureMode() ||*/ cmd->Get_arglength() > 100 && full_arguments.size()) {
		CommandLine* temp = new CommandLine(cmd->GetFileName(),full_arguments.c_str());
		delete cmd;
		cmd = temp;
	}
	full_arguments.assign(""); //Clear so it gets even more save
}

static char last_written_character = 0;//For 0xA to OxD 0xA expansion
void Program::WriteOut(const char * format,...) {
	char buf[2048];
	va_list msg;
	
	va_start(msg,format);
	vsnprintf(buf,2047,format,msg);
	va_end(msg);

	uint16_t size = (uint16_t)strlen(buf);
	dos.internal_output=true;
	for(uint16_t i = 0; i < size;i++) {
		uint8_t out;uint16_t s=1;
		if (buf[i] == 0xA && last_written_character != 0xD) {
			out = 0xD;DOS_WriteFile(STDOUT,&out,&s);
		}
		last_written_character = (char)(out = (uint8_t)buf[i]);
		DOS_WriteFile(STDOUT,&out,&s);
	}
	dos.internal_output=false;
	
//	DOS_WriteFile(STDOUT,(uint8_t *)buf,&size);
}

void Program::WriteOut_NoParsing(const char * format) {
	uint16_t size = (uint16_t)strlen(format);
	char const* buf = format;
	dos.internal_output=true;
	for(uint16_t i = 0; i < size;i++) {
		uint8_t out;uint16_t s=1;
		if (buf[i] == 0xA && last_written_character != 0xD) {
			out = 0xD;DOS_WriteFile(STDOUT,&out,&s);
		}
		last_written_character = (char)(out = (uint8_t)buf[i]);
		DOS_WriteFile(STDOUT,&out,&s);
	}
	dos.internal_output=false;

//	DOS_WriteFile(STDOUT,(uint8_t *)format,&size);
}

static bool LocateEnvironmentBlock(PhysPt &env_base,PhysPt &env_fence,Bitu env_seg) {
	if (env_seg == 0) {
		/* The DOS program might have freed it's environment block perhaps. */
		return false;
	}

	DOS_MCB env_mcb((uint16_t)(env_seg-1)); /* read the environment block's MCB to determine how large it is */
	env_base = PhysMake((uint16_t)env_seg,0);
	env_fence = env_base + (PhysPt)(env_mcb.GetSize() << 4u);
	return true;
}

int EnvPhys_StrCmp(PhysPt es,PhysPt ef,const char *ls) {
    (void)ef;//UNUSED
	while (1) {
		unsigned char a = mem_readb(es++);
		unsigned char b = (unsigned char)(*ls++);
		if (a == '=') a = 0;
		if (a == 0 && b == 0) break;
		if (a == b) continue;
		return (int)a - (int)b;
	}

	return 0;
}

void EnvPhys_StrCpyToCPPString(std::string &result,PhysPt &env_scan,PhysPt env_fence) {
	char tmp[512],*w=tmp,*wf=tmp+sizeof(tmp)-1;

	result.clear();
	while (env_scan < env_fence) {
		char c;
		if ((c=(char)mem_readb(env_scan++)) == 0) break;

		if (w >= wf) {
			*w = 0;
			result += tmp;
			w = tmp;
		}

		assert(w < wf);
		*w++ = c;
	}
	if (w != tmp) {
		*w = 0;
		result += tmp;
	}
}

bool EnvPhys_ScanUntilNextString(PhysPt &env_scan,PhysPt env_fence) {
	/* scan until end of block or NUL */
	while (env_scan < env_fence && mem_readb(env_scan) != 0) env_scan++;

	/* if we hit the fence, that's something to warn about. */
	if (env_scan >= env_fence) {
		LOG_MSG("Warning: environment string scan hit the end of the environment block without terminating NUL\n");
		return false;
	}

	/* if we stopped at anything other than a NUL, that's something to warn about */
	if (mem_readb(env_scan) != 0) {
		LOG_MSG("Warning: environment string scan scan stopped without hitting NUL\n");
		return false;
	}

	env_scan++; /* skip NUL */
	return true;
}

bool Program::GetEnvStr(const char * entry,std::string & result) {
	PhysPt env_base,env_fence,env_scan;

	if (dos_kernel_disabled) {
		LOG_MSG("BUG: Program::GetEnvNum() called with DOS kernel disabled (such as OS boot).\n");
		return false;
	}

	if (!LocateEnvironmentBlock(env_base,env_fence,psp->GetEnvironment())) {
		LOG_MSG("Warning: GetEnvCount() was not able to locate the program's environment block\n");
		return false;
	}

	std::string bigentry(entry);
	for (std::string::iterator it = bigentry.begin(); it != bigentry.end(); ++it) *it = toupper(*it);

	env_scan = env_base;
	while (env_scan < env_fence) {
		/* "NAME" + "=" + "VALUE" + "\0" */
		/* end of the block is a NULL string meaning a \0 follows the last string's \0 */
		if (mem_readb(env_scan) == 0) break; /* normal end of block */

		if (EnvPhys_StrCmp(env_scan,env_fence,bigentry.c_str()) == 0) {
			EnvPhys_StrCpyToCPPString(result,env_scan,env_fence);
			return true;
		}

		if (!EnvPhys_ScanUntilNextString(env_scan,env_fence)) break;
	}

	return false;
}

bool Program::GetEnvNum(Bitu want_num,std::string & result) {
	PhysPt env_base,env_fence,env_scan;
	Bitu num = 0;

	if (dos_kernel_disabled) {
		LOG_MSG("BUG: Program::GetEnvNum() called with DOS kernel disabled (such as OS boot).\n");
		return false;
	}

	if (!LocateEnvironmentBlock(env_base,env_fence,psp->GetEnvironment())) {
		LOG_MSG("Warning: GetEnvCount() was not able to locate the program's environment block\n");
		return false;
	}

	result.clear();
	env_scan = env_base;
	while (env_scan < env_fence) {
		/* "NAME" + "=" + "VALUE" + "\0" */
		/* end of the block is a NULL string meaning a \0 follows the last string's \0 */
		if (mem_readb(env_scan) == 0) break; /* normal end of block */

		if (num == want_num) {
			EnvPhys_StrCpyToCPPString(result,env_scan,env_fence);
			return true;
		}

		num++;
		if (!EnvPhys_ScanUntilNextString(env_scan,env_fence)) break;
	}

	return false;
}

Bitu Program::GetEnvCount(void) {
	PhysPt env_base,env_fence,env_scan;
	Bitu num = 0;

	if (dos_kernel_disabled) {
		LOG_MSG("BUG: Program::GetEnvCount() called with DOS kernel disabled (such as OS boot).\n");
		return 0;
	}

	if (!LocateEnvironmentBlock(env_base,env_fence,psp->GetEnvironment())) {
		LOG_MSG("Warning: GetEnvCount() was not able to locate the program's environment block\n");
		return false;
	}

	env_scan = env_base;
	while (env_scan < env_fence) {
		/* "NAME" + "=" + "VALUE" + "\0" */
		/* end of the block is a NULL string meaning a \0 follows the last string's \0 */
		if (mem_readb(env_scan++) == 0) break; /* normal end of block */
		num++;
		if (!EnvPhys_ScanUntilNextString(env_scan,env_fence)) break;
	}

	return num;
}

void Program::DebugDumpEnv() {
	PhysPt env_base,env_fence,env_scan;
	unsigned char c;
	std::string tmp;

	if (dos_kernel_disabled)
		return;

	if (!LocateEnvironmentBlock(env_base,env_fence,psp->GetEnvironment()))
		return;

	env_scan = env_base;
	LOG_MSG("DebugDumpEnv()");
	while (env_scan < env_fence) {
		if (mem_readb(env_scan) == 0) break;

		while (env_scan < env_fence) {
			if ((c=mem_readb(env_scan++)) == 0) break;
			tmp += (char)c;
		}

		LOG_MSG("...%s",tmp.c_str());
		tmp = "";
	}
}

/* NTS: "entry" string must have already been converted to uppercase */
bool Program::SetEnv(const char * entry,const char * new_string) {
	PhysPt env_base,env_fence,env_scan;
	size_t nsl = 0,el = 0,needs;

	if (dos_kernel_disabled) {
		LOG_MSG("BUG: Program::SetEnv() called with DOS kernel disabled (such as OS boot).\n");
		return false;
	}

	if (!LocateEnvironmentBlock(env_base,env_fence,psp->GetEnvironment())) {
		LOG_MSG("Warning: SetEnv() was not able to locate the program's environment block\n");
		return false;
	}

	std::string bigentry(entry);
	for (std::string::iterator it = bigentry.begin(); it != bigentry.end(); ++it) *it = toupper(*it);

	el = strlen(bigentry.c_str());
	if (*new_string != 0) nsl = strlen(new_string);
	needs = nsl+1+el+1+1; /* entry + '=' + new_string + '\0' + '\0' */

	/* look for the variable in the block. break the loop if found */
	env_scan = env_base;
	while (env_scan < env_fence) {
		if (mem_readb(env_scan) == 0) break;

		if (EnvPhys_StrCmp(env_scan,env_fence,bigentry.c_str()) == 0) {
			/* found it. remove by shifting the rest of the environment block over */
			int zeroes=0;
			PhysPt s,d;

			/* before we remove it: is there room for the new value? */
			if (nsl != 0) {
				if ((env_scan+needs) > env_fence) {
					LOG_MSG("Program::SetEnv() error, insufficient room for environment variable %s=%s (replacement)\n",bigentry.c_str(),new_string);
					DebugDumpEnv();
					return false;
				}
			}

			s = env_scan; d = env_scan;
			while (s < env_fence && mem_readb(s) != 0) s++;
			if (s < env_fence && mem_readb(s) == 0) s++;

			while (s < env_fence) {
				unsigned char b = mem_readb(s++);

				if (b == 0) zeroes++;
				else zeroes=0;

				mem_writeb(d++,b);
				if (zeroes >= 2) break; /* two consecutive zeros means the end of the block */
			}
		}
		else {
			/* scan to next string */
			if (!EnvPhys_ScanUntilNextString(env_scan,env_fence)) break;
		}
	}

	/* At this point, env_scan points to the first byte beyond the block */
	/* add the string to the end of the block */
	if (*new_string != 0) {
		if ((env_scan+needs) > env_fence) {
			LOG_MSG("Program::SetEnv() error, insufficient room for environment variable %s=%s (addition)\n",bigentry.c_str(),new_string);
			DebugDumpEnv();
			return false;
		}

		assert(env_scan < env_fence);
		for (const char *s=bigentry.c_str();*s != 0;) mem_writeb(env_scan++,(uint8_t)(*s++));
		mem_writeb(env_scan++,'=');

		assert(env_scan < env_fence);
		for (const char *s=new_string;*s != 0;) mem_writeb(env_scan++,(uint8_t)(*s++));
		mem_writeb(env_scan++,0);
		mem_writeb(env_scan++,0);

		assert(env_scan <= env_fence);
	}

	return true;
}

bool MSG_Write(const char *);

/*! \brief          CONFIG.COM utility to control configuration and files
 *
 *  \description    Utility to write configuration, set configuration,
 *                  and other configuration related functions.
 */
class CONFIG : public Program {
public:
    /*! \brief      Program entry point, when the command is run
     */
	void Run(void);
private:
	void restart(const char* useconfig);
	
	void writeconf(std::string name, bool configdir,int everything, bool norem) {
		// "config -wcd" should write to the config directory
		if (configdir) {
			// write file to the default config directory
			std::string config_path;
			Cross::GetPlatformConfigDir(config_path);
			struct stat info;
			if (!stat(config_path.c_str(), &info) || !(info.st_mode & S_IFDIR)) {
#ifdef WIN32
				CreateDirectory(config_path.c_str(), NULL);
#else
				mkdir(config_path.c_str(), 0755);
#endif
			}
			name = config_path + name;
		}
		WriteOut(MSG_Get("PROGRAM_CONFIG_FILE_WHICH"),name.c_str());
		if (!control->PrintConfig(name.c_str(),everything,norem)) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_FILE_ERROR"),name.c_str());
		}
		return;
	}
	
	bool securemode_check() {
		if (control->SecureMode()) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
			return true;
		}
		return false;
	}
};

void dos_ver_menu(bool start), ReloadMapper(Section_prop *sec, bool init), SetGameState_Run(int value), update_dos_ems_menu(void), MountAllDrives(Program * program), GFX_SwitchFullScreen(void), RebootConfig(std::string filename, bool confirm=false);
bool set_ver(char *s), GFX_IsFullscreen(void);

void CONFIG::Run(void) {
	static const char* const params[] = {
		"-r", "-wcp", "-wcd", "-wc", "-writeconf", "-wcpboot", "-wcdboot", "-wcboot", "-writeconfboot", "-bootconf", "-bc",
		"-l", "-rmconf", "-h", "-help", "-?", "-axclear", "-axadd", "-axtype",
		"-avistart","-avistop",
		"-startmapper",
		"-get", "-set",
		"-writelang", "-wl", "-securemode", "-setup", "-all", "-mod", "-norem", "-errtest", "-gui", NULL };
	enum prs {
		P_NOMATCH, P_NOPARAMS, // fixed return values for GetParameterFromList
		P_RESTART,
		P_WRITECONF_PORTABLE, P_WRITECONF_DEFAULT, P_WRITECONF, P_WRITECONF2,
		P_WRITECONF_PORTABLE_REBOOT, P_WRITECONF_DEFAULT_REBOOT, P_WRITECONF_REBOOT, P_WRITECONF2_REBOOT,
		P_BOOTCONF, P_BOOTCONF2, P_LISTCONF, P_KILLCONF,
		P_HELP, P_HELP2, P_HELP3,
		P_AUTOEXEC_CLEAR, P_AUTOEXEC_ADD, P_AUTOEXEC_TYPE,
		P_REC_AVI_START, P_REC_AVI_STOP,
		P_START_MAPPER,
		P_GETPROP, P_SETPROP,
		P_WRITELANG, P_WRITELANG2,
		P_SECURE, P_SETUP, P_ALL, P_MOD, P_NOREM, P_ERRTEST, P_GUI
	} presult = P_NOMATCH;

	Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));
	int all = section->Get_bool("show advanced options")?1:-1;
	bool first = true, norem = false;
	std::vector<std::string> pvars;
	if (cmd->FindExist("-setup", true)) all = 2;
	else if (cmd->FindExist("-all", true)) all = 1;
    else if (cmd->FindExist("-mod", true)) all = 0;
    if (cmd->FindExist("-norem", true)) norem = true;
	// Loop through the passed parameters
	while(presult != P_NOPARAMS) {
		presult = (enum prs)cmd->GetParameterFromList(params, pvars);
		switch(presult) {
	
		case P_SETUP:
			all = 2;
			break;

		case P_ALL:
			if (all<1) all = 1;
			break;

		case P_MOD:
			if (all==-1) all = 0;
			break;

		case P_NOREM:
			norem = true;
			break;

		case P_GUI:
			void GUI_Run(bool pressed);
			GUI_Run(false);
			break;

		case P_ERRTEST:
			exit_status = 1;
			WriteExitStatus();
			return;

		case P_RESTART:
            WriteOut("-restart is no longer supported\n");
			return;
		
		case P_LISTCONF: {
			Bitu size = (Bitu)control->configfiles.size();
			std::string config_path;
			Cross::GetPlatformConfigDir(config_path);
			WriteOut(MSG_Get("PROGRAM_CONFIG_CONFDIR"), VERSION,config_path.c_str());
			char cwd[512] = {0};
			char *res = getcwd(cwd,sizeof(cwd)-1);
			if (res!=NULL) WriteOut(MSG_Get("PROGRAM_CONFIG_WORKDIR"), cwd);
			if (size==0) WriteOut(MSG_Get("PROGRAM_CONFIG_NOCONFIGFILE"));
			else {
				WriteOut(MSG_Get("PROGRAM_CONFIG_PRIMARY_CONF"),control->configfiles.front().c_str());
				if (size > 1) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_ADDITIONAL_CONF"));
					for(Bitu i = 1; i < size; i++)
						WriteOut("%s\n",control->configfiles[i].c_str());
				}
			}
			if (control->startup_params.size() > 0) {
				std::string test;
				for(size_t k = 0; k < control->startup_params.size(); k++)
					test += control->startup_params[k] + " ";
				WriteOut(MSG_Get("PROGRAM_CONFIG_PRINT_STARTUP"), test.c_str());
			}
			break;
		}
		case P_WRITECONF: case P_WRITECONF2: case P_WRITECONF_REBOOT: case P_WRITECONF2_REBOOT:
			if (securemode_check()) return;
			if (pvars.size() > 1) return;
			else if (pvars.size() == 1) {
				// write config to specific file, except if it is an absolute path
				writeconf(pvars[0], !Cross::IsPathAbsolute(pvars[0]), all, norem);
				if (presult==P_WRITECONF_REBOOT || presult==P_WRITECONF2_REBOOT) RebootConfig(pvars[0]);
			} else {
				// -wc without parameter: write primary config file
				if (control->configfiles.size()) {
					writeconf(control->configfiles[0], false, all, norem);
					if (presult==P_WRITECONF_REBOOT || presult==P_WRITECONF2_REBOOT) RebootConfig(control->configfiles[0]);
				} else WriteOut(MSG_Get("PROGRAM_CONFIG_NOCONFIGFILE"));
			}
			break;
		case P_WRITECONF_DEFAULT: case P_WRITECONF_DEFAULT_REBOOT: {
			// write to /userdir/dosbox-x-0.xx.conf
			if (securemode_check()) return;
			if (pvars.size() > 0) return;
			std::string confname;
			Cross::GetPlatformConfigName(confname);
			writeconf(confname, true, all, norem);
			if (presult==P_WRITECONF_DEFAULT_REBOOT) RebootConfig(confname);
			break;
		}
		case P_WRITECONF_PORTABLE: case P_WRITECONF_PORTABLE_REBOOT:
			if (securemode_check()) return;
			if (pvars.size() > 1) return;
			else if (pvars.size() == 1) {
				// write config to startup directory
				writeconf(pvars[0], false, all, norem);
				if (presult==P_WRITECONF_PORTABLE_REBOOT) RebootConfig(pvars[0]);
			} else {
				// -wcp without parameter: write dosbox-x.conf to startup directory
				writeconf(std::string("dosbox-x.conf"), false, all, norem);
				if (presult==P_WRITECONF_PORTABLE_REBOOT) RebootConfig(std::string("dosbox-x.conf"));
			}
			break;
		case P_BOOTCONF: case P_BOOTCONF2:
			if (securemode_check()) return;
			if (pvars.size() > 1) return;
			else if (pvars.size() == 1) {
				RebootConfig(pvars[0]);
			} else {
				Bitu size = (Bitu)control->configfiles.size();
				if (size==0) RebootConfig("dosbox-x.conf");
				else RebootConfig(control->configfiles.front().c_str());
            }
			break;
		case P_NOPARAMS:
			if (!first) break;

		case P_NOMATCH:
			WriteOut(MSG_Get("PROGRAM_CONFIG_USAGE"));
			return;

		case P_HELP: case P_HELP2: case P_HELP3: {
			switch(pvars.size()) {
			case 0:
				WriteOut(MSG_Get("PROGRAM_CONFIG_USAGE"));
				return;
			case 1: {
				if (!strcasecmp("sections",pvars[0].c_str())) {
					// list the sections
					WriteOut(MSG_Get("PROGRAM_CONFIG_HLP_SECTLIST"));
					int i = 0;
					while(true) {
						Section* sec = control->GetSection(i++);
						if (!sec) break;
						WriteOut("%s\n",sec->GetName());
					}
					return;
				}
				// if it's a section, leave it as one-param
				Section* sec = control->GetSection(pvars[0].c_str());
				if (!sec) {
					// could be a property
					sec = control->GetSectionFromProperty(pvars[0].c_str());
					if (!sec) {
						WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"));
						return;
					}
					pvars.insert(pvars.begin(),std::string(sec->GetName()));
				}
				break;
			}
			case 2: {
				// sanity check
				Section* sec = control->GetSection(pvars[0].c_str());
				Section* sec2 = control->GetSectionFromProperty(pvars[1].c_str());
				
				if (sec != sec2) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_DUPLICATE"));
				}
				break;
			}
			default:
				WriteOut(MSG_Get("PROGRAM_CONFIG_USAGE"));
				return;
			}	
			// if we have one value in pvars, it's a section
			// two values are section + property
			Section* sec = control->GetSection(pvars[0].c_str());
			if (sec==NULL) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"));
				return;
			}
			Section_prop* psec = dynamic_cast <Section_prop*>(sec);
			if (psec==NULL) {
				// failed; maybe it's the autoexec section?
				Section_line* pline = dynamic_cast <Section_line*>(sec);
				if (pline==NULL) E_Exit("Section dynamic cast failed.");

				WriteOut(MSG_Get("PROGRAM_CONFIG_HLP_LINEHLP"),
					pline->GetName(),
					// this is 'unclean' but the autoexec section has no help associated
					MSG_Get("AUTOEXEC_CONFIGFILE_HELP"),
					pline->data.c_str() );
				return;
			} 
			if (pvars.size()==1) {
				size_t i = 0;
				WriteOut(MSG_Get("PROGRAM_CONFIG_HLP_SECTHLP"),pvars[0].c_str());
				while(true) {
					// list the properties
					Property* p = psec->Get_prop((int)(i++));
					if (p==NULL) break;
                    if (!(all>0 || all==-1 && (p->basic() || p->modified()) || !all && (p->propname == "rem" && (!strcmp(pvars[0].c_str(), "4dos") || !strcmp(pvars[0].c_str(), "config")) || p->modified()))) continue;
					WriteOut("%s\n", p->propname.c_str());
				}
				if (!strcasecmp(pvars[0].c_str(), "config"))
					WriteOut("set\ninstall\ninstallhigh\ndevice\ndevicehigh\n");
			} else {
				// find the property by it's name
				size_t i = 0;
				while (true) {
					Property *p = psec->Get_prop((int)(i++));
					if (p==NULL) break;
					if (!strcasecmp(p->propname.c_str(),pvars[1].c_str())) {
						// found it; make the list of possible values
						std::string propvalues;
						std::vector<Value> pv = p->GetValues();
						
						if (p->Get_type()==Value::V_BOOL) {
							// possible values for boolean are true, false
							propvalues += "true, false";
						} else if (p->Get_type()==Value::V_INT) {
							// print min, max for integer values if used
							Prop_int* pint = dynamic_cast <Prop_int*>(p);
							if (pint==NULL) E_Exit("Int property dynamic cast failed.");
							if (pint->getMin() != pint->getMax()) {
								std::ostringstream oss;
								oss << pint->getMin();
								oss << "..";
								oss << pint->getMax();
								propvalues += oss.str();
							}
						}
						for(Bitu k = 0; k < pv.size(); k++) {
							if (pv[k].ToString() =="%u")
								propvalues += MSG_Get("PROGRAM_CONFIG_HLP_POSINT");
							else propvalues += pv[k].ToString();
							if ((k+1) < pv.size()) propvalues += ", ";
						}

						WriteOut(MSG_Get("PROGRAM_CONFIG_HLP_PROPHLP"),
							p->propname.c_str(),
							sec->GetName(),
							p->Get_help(),propvalues.c_str(),
							p->Get_Default_Value().ToString().c_str(),
							p->GetValue().ToString().c_str());
						// print 'changability'
						if (p->getChange()==Property::Changeable::OnlyAtStart) {
							WriteOut(MSG_Get("PROGRAM_CONFIG_HLP_NOCHANGE"));
						}
						return;
					}
				}
				break;
			}
			return;
		}
		case P_AUTOEXEC_CLEAR: {
			Section_line* sec = dynamic_cast <Section_line*>
				(control->GetSection(std::string("autoexec")));
			sec->data.clear();
			break;
		}
		case P_AUTOEXEC_ADD: {
			if (pvars.size() == 0) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_MISSINGPARAM"));
				return;
			}
			Section_line* sec = dynamic_cast <Section_line*>
				(control->GetSection(std::string("autoexec")));

			for(Bitu i = 0; i < pvars.size(); i++) sec->HandleInputline(pvars[i]);
			break;
		}
		case P_AUTOEXEC_TYPE: {
			Section_line* sec = dynamic_cast <Section_line*>
				(control->GetSection(std::string("autoexec")));
			WriteOut("\n%s",sec->data.c_str());
			break;
		}
		case P_REC_AVI_START:
			CAPTURE_VideoStart();
			break;
		case P_REC_AVI_STOP:
			CAPTURE_VideoStop();
			break;
		case P_START_MAPPER:
			if (securemode_check()) return;
			MAPPER_Run(false);
			break;
		case P_GETPROP: {
			// "section property"
			// "property"
			// "section"
			// "section" "property"
			if (pvars.size()==0) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_GET_SYNTAX"));
				return;
			}
			std::string::size_type spcpos = pvars[0].find_first_of(' ');
			// split on the ' '
			if (spcpos != std::string::npos) {
				if (spcpos>1&&pvars[0].c_str()[spcpos-1]==',')
					spcpos=pvars[0].find_first_of(' ', spcpos+1);
				if (spcpos != std::string::npos) {
					pvars.insert(pvars.begin()+1,pvars[0].substr(spcpos+1));
					pvars[0].erase(spcpos);
				}
			}
			switch(pvars.size()) {
			case 1: {
				// property/section only
				// is it a section?
				Section* sec = control->GetSection(pvars[0].c_str());
				if (sec) {
					// list properties in section
					int i = 0;
					Section_prop* psec = dynamic_cast <Section_prop*>(sec);
					if (psec==NULL) {
						// autoexec section
						Section_line* pline = dynamic_cast <Section_line*>(sec);
						if (pline==NULL) E_Exit("Section dynamic cast failed.");

						WriteOut("%s",pline->data.c_str());
						break;
					}
					while(true) {
						// list the properties
						Property* p = psec->Get_prop(i++);
						if (p==NULL) break;
                        if (!(all>0 || all==-1 && (p->basic() || p->modified()) || !all && (p->propname == "rem" && (!strcmp(pvars[0].c_str(), "4dos") || !strcmp(pvars[0].c_str(), "config")) || p->modified()))) continue;
						WriteOut("%s=%s\n", p->propname.c_str(),
							p->GetValue().ToString().c_str());
					}
					if (!strcasecmp(pvars[0].c_str(), "config")||!strcasecmp(pvars[0].c_str(), "4dos")) {
						const char * extra = const_cast<char*>(psec->data.c_str());
						if (extra&&strlen(extra)) {
							std::istringstream in(extra);
							char linestr[CROSS_LEN+1], cmdstr[CROSS_LEN], valstr[CROSS_LEN];
							char *cmd=cmdstr, *val=valstr, /**lin=linestr,*/ *p;
							if (in)	for (std::string line; std::getline(in, line); ) {
								if (line.length()>CROSS_LEN) {
									strncpy(linestr, line.c_str(), CROSS_LEN);
									linestr[CROSS_LEN]=0;
								} else
									strcpy(linestr, line.c_str());
								p=strchr(linestr, '=');
								if (p!=NULL) {
									*p=0;
									strcpy(cmd, linestr);
									cmd=trim(cmd);
									strcpy(val, p+1);
									val=trim(val);
									lowcase(cmd);
									if (!strcasecmp(pvars[0].c_str(), "4dos")||!strncmp(cmd, "set ", 4)||!strcmp(cmd, "install")||!strcmp(cmd, "installhigh")||!strcmp(cmd, "device")||!strcmp(cmd, "devicehigh"))
                                        if (!((!strcmp(cmd, "install")||!strcmp(cmd, "installhigh")||!strcmp(cmd, "device")||!strcmp(cmd, "devicehigh"))&&!strlen(val)&&!all))
                                            WriteOut("%s=%s\n", cmd, val);
								}
							}
						}
					}
				} else {
					// no: maybe it's a property?
					sec = control->GetSectionFromProperty(pvars[0].c_str());
					if (!sec) {
                        int maxWidth, maxHeight;
                        void GetMaxWidthHeight(int *pmaxWidth, int *pmaxHeight), GetDrawWidthHeight(int *pdrawWidth, int *pdrawHeight);
                        if (!strcasecmp(pvars[0].c_str(), "screenwidth")) {
                            GetMaxWidthHeight(&maxWidth, &maxHeight);
                            WriteOut("%d\n",maxWidth);
                            first_shell->SetEnv("CONFIG",std::to_string(maxWidth).c_str());
                        } else if (!strcasecmp(pvars[0].c_str(), "screenheight")) {
                            GetMaxWidthHeight(&maxWidth, &maxHeight);
                            WriteOut("%d\n",maxHeight);
                            first_shell->SetEnv("CONFIG",std::to_string(maxHeight).c_str());
                        } else if (!strcasecmp(pvars[0].c_str(), "drawwidth")) {
                            GetDrawWidthHeight(&maxWidth, &maxHeight);
                            WriteOut("%d\n",maxWidth);
                            first_shell->SetEnv("CONFIG",std::to_string(maxWidth).c_str());
                        } else if (!strcasecmp(pvars[0].c_str(), "drawheight")) {
                            GetDrawWidthHeight(&maxWidth, &maxHeight);
                            WriteOut("%d\n",maxHeight);
                            first_shell->SetEnv("CONFIG",std::to_string(maxHeight).c_str());
#if defined(C_SDL2)
                        } else if (!strcasecmp(pvars[0].c_str(), "clientwidth")) {
                            int w = 640,h = 480;
                            SDL_Window* GFX_GetSDLWindow(void);
                            SDL_GetWindowSize(GFX_GetSDLWindow(), &w, &h);
                            WriteOut("%d\n",w);
                            first_shell->SetEnv("CONFIG",std::to_string(w).c_str());
                        } else if (!strcasecmp(pvars[0].c_str(), "clientheight")) {
                            int w = 640,h = 480;
                            SDL_Window* GFX_GetSDLWindow(void);
                            SDL_GetWindowSize(GFX_GetSDLWindow(), &w, &h);
                            WriteOut("%d\n",h);
                            first_shell->SetEnv("CONFIG",std::to_string(h).c_str());
#elif defined(WIN32)
                        } else if (!strcasecmp(pvars[0].c_str(), "clientwidth")) {
                            RECT rect;
                            GetClientRect(GetHWND(), &rect);
                            WriteOut("%d\n",rect.right-rect.left);
                            first_shell->SetEnv("CONFIG",std::to_string(rect.right-rect.left).c_str());
                        } else if (!strcasecmp(pvars[0].c_str(), "clientheight")) {
                            RECT rect;
                            GetClientRect(GetHWND(), &rect);
                            WriteOut("%d\n",rect.bottom-rect.top);
                            first_shell->SetEnv("CONFIG",std::to_string(rect.bottom-rect.top).c_str());
#endif
#if defined(WIN32)
                        } else if (!strcasecmp(pvars[0].c_str(), "windowwidth")) {
                            RECT rect;
                            GetWindowRect(GetHWND(), &rect);
                            WriteOut("%d\n",rect.right-rect.left);
                            first_shell->SetEnv("CONFIG",std::to_string(rect.right-rect.left).c_str());
                        } else if (!strcasecmp(pvars[0].c_str(), "windowheight")) {
                            RECT rect;
                            GetWindowRect(GetHWND(), &rect);
                            WriteOut("%d\n",rect.bottom-rect.top);
                            first_shell->SetEnv("CONFIG",std::to_string(rect.bottom-rect.top).c_str());
#endif
                        } else
                            WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"));
						return;
					}
					// it's a property name
					std::string val = sec->GetPropValue(pvars[0].c_str());
					WriteOut("%s\n",val.c_str());
					first_shell->SetEnv("CONFIG",val.c_str());
				}
				break;
			}
			case 2: {
				// section + property
				Section* sec = control->GetSection(pvars[0].c_str());
				if (!sec) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_ERROR"), pvars[0].c_str());
					return;
				}
				std::string val = sec->GetPropValue(pvars[1].c_str());
				if (val == NO_SUCH_PROPERTY) {
					if (!strcasecmp(pvars[0].c_str(), "config") && (!strcasecmp(pvars[1].c_str(), "set") || !strcasecmp(pvars[1].c_str(), "device") || !strcasecmp(pvars[1].c_str(), "devicehigh") || !strcasecmp(pvars[1].c_str(), "install") || !strcasecmp(pvars[1].c_str(), "installhigh"))) {
						Section_prop* psec = dynamic_cast <Section_prop*>(sec);
						const char * extra = const_cast<char*>(psec->data.c_str());
						if (extra&&strlen(extra)) {
							std::istringstream in(extra);
							char linestr[CROSS_LEN+1], cmdstr[CROSS_LEN], valstr[CROSS_LEN];
							char *cmd=cmdstr, *val=valstr, /**lin=linestr,*/ *p;
							if (in)	for (std::string line; std::getline(in, line); ) {
								if (line.length()>CROSS_LEN) {
									strncpy(linestr, line.c_str(), CROSS_LEN);
									linestr[CROSS_LEN]=0;
								} else
									strcpy(linestr, line.c_str());
								p=strchr(linestr, '=');
								if (p!=NULL) {
									*p=0;
									strcpy(cmd, linestr);
									cmd=trim(cmd);
									strcpy(val, p+1);
									val=trim(val);
									lowcase(cmd);
									if (!strncasecmp(cmd, "set ", 4)&&!strcasecmp(pvars[1].c_str(), "set"))
										WriteOut("%s=%s\n", trim(cmd+4), val);
									else if(!strcasecmp(cmd, pvars[1].c_str()))
										WriteOut("%s\n", val);
								}
							}
						}
					} else
						WriteOut(MSG_Get("PROGRAM_CONFIG_NO_PROPERTY"), pvars[1].c_str(),pvars[0].c_str());   
					return;
				}
				WriteOut("%s\n",val.c_str());
                first_shell->SetEnv("CONFIG",val.c_str());
                break;
			}
			default:
				WriteOut(MSG_Get("PROGRAM_CONFIG_GET_SYNTAX"));
				return;
			}
			return;
		}
		case P_SETPROP:	{
			// Code for the configuration changes
			// Official format: config -set "section property=value"
			// Accepted: with or without -set, 
			// "section property=value"
			// "property" "value"
			// "section" "property=value"
			// "section" "property=value" "value" "value" ...
			// "section" "property" "value"
			// "section" "property" "value" "value" ...
			// "section property" "value" "value" ...
			// "property" "value" "value" ...
			// "property=value" "value" "value" ...

			if (pvars.size()==0) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_SET_SYNTAX"));
				return;
			}

			// add rest of command
			std::string rest;
			if (cmd->GetStringRemain(rest)) pvars.push_back(rest);

			// attempt to split off the first word
			std::string::size_type spcpos = pvars[0].find_first_of(' ');
			if (spcpos>1&&pvars[0].c_str()[spcpos-1]==',')
				spcpos=pvars[0].find_first_of(' ', spcpos+1);

			std::string::size_type equpos = pvars[0].find_first_of('=');

			if ((equpos != std::string::npos) && 
				((spcpos == std::string::npos) || (equpos < spcpos))) {
				// If we have a '=' possibly before a ' ' split on the =
				pvars.insert(pvars.begin()+1,pvars[0].substr(equpos+1));
				pvars[0].erase(equpos);
				// As we had a = the first thing must be a property now
				Section* sec=control->GetSectionFromProperty(pvars[0].c_str());
				if (sec) pvars.insert(pvars.begin(),std::string(sec->GetName()));
				else {
					WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"));
					return;
				}
				// order in the vector should be ok now
			} else {
				if (equpos != std::string::npos && spcpos < equpos) {
					// ' ' before a possible '=', split on the ' '
					pvars.insert(pvars.begin()+1,pvars[0].substr(spcpos+1));
					pvars[0].erase(spcpos);
				}
				// check if the first parameter is a section or property
				Section* sec = control->GetSection(pvars[0].c_str());
				if (!sec) {
					// not a section: little duplicate from above
					sec=control->GetSectionFromProperty(pvars[0].c_str());
					if (sec) pvars.insert(pvars.begin(),std::string(sec->GetName()));
					else {
						WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"));
						return;
					}
				} else {
					// first of pvars is most likely a section, but could still be gus
					// have a look at the second parameter
					if (pvars.size() < 2) {
						WriteOut(MSG_Get("PROGRAM_CONFIG_SET_SYNTAX"));
						return;
					}
					std::string::size_type equpos2 = pvars[1].find_first_of('=');
					if (equpos2 != std::string::npos) {
						// split on the =
						pvars.insert(pvars.begin()+2,pvars[1].substr(equpos2+1));
						pvars[1].erase(equpos2);
					}
					// is this a property?
					Section* sec2 = control->GetSectionFromProperty(pvars[1].c_str());
					if (!sec2) {
						// not a property, 
						Section* sec3 = control->GetSectionFromProperty(pvars[0].c_str());
						if (sec3) {
							// section and property name are identical
							pvars.insert(pvars.begin(),pvars[0]);
						} // else has been checked above already
					}
				}
			}
			if(pvars.size() < 3) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_SET_SYNTAX"));
				return;
			}
			// check if the property actually exists in the section
			Section* sec2 = control->GetSectionFromProperty(pvars[1].c_str());
			if (!sec2) {
				if (!strcasecmp(pvars[0].c_str(), "config") && (!strcasecmp(pvars[1].c_str(), "set") || !strcasecmp(pvars[1].c_str(), "device") || !strcasecmp(pvars[1].c_str(), "devicehigh") || !strcasecmp(pvars[1].c_str(), "install") || !strcasecmp(pvars[1].c_str(), "installhigh")))
					WriteOut("Cannot set property %s in section %s.\n", pvars[1].c_str(),pvars[0].c_str());
				else
					WriteOut(MSG_Get("PROGRAM_CONFIG_NO_PROPERTY"), pvars[1].c_str(),pvars[0].c_str());
				return;
			}
			// Input has been parsed (pvar[0]=section, [1]=property, [2]=value)
			// now execute
			Section* tsec = control->GetSection(pvars[0]);
			std::string value(pvars[2]);
			//Due to parsing there can be a = at the start of value.
			while (value.size() && (value.at(0) ==' ' ||value.at(0) =='=') ) value.erase(0,1);
			for(Bitu i = 3; i < pvars.size(); i++) value += (std::string(" ") + pvars[i]);
			std::string inputline = pvars[1] + "=" + value;
			
			bool change_success = tsec->HandleInputline(inputline.c_str());
			if (change_success) {
				if (!strcasecmp(pvars[0].c_str(), "dosbox")||!strcasecmp(pvars[0].c_str(), "dos")||!strcasecmp(pvars[0].c_str(), "sdl")||!strcasecmp(pvars[0].c_str(), "render")) {
					Section_prop *section = static_cast<Section_prop *>(control->GetSection(pvars[0].c_str()));
					if (section != NULL) {
						if (!strcasecmp(pvars[0].c_str(), "dosbox")) {
							force_nocachedir = section->Get_bool("nocachedir");
                            sync_time = section->Get_bool("synchronize time");
                            if (!strcasecmp(inputline.substr(0, 17).c_str(), "synchronize time=")) {
                                manualtime=false;
                                mainMenu.get_item("sync_host_datetime").check(sync_time).refresh_item(mainMenu);
                            }
							std::string freesizestr = section->Get_string("freesizecap");
                            if (freesizestr == "fixed" || freesizestr == "false" || freesizestr == "0") freesizecap = 0;
                            else if (freesizestr == "relative" || freesizestr == "2") freesizecap = 2;
                            else freesizecap = 1;
							wpcolon = section->Get_bool("leading colon write protect image");
							lockmount = section->Get_bool("locking disk image mount");
							if (!strcasecmp(inputline.substr(0, 9).c_str(), "saveslot=")) SetGameState_Run(section->Get_int("saveslot")-1);
                            if (!strcasecmp(inputline.substr(0, 11).c_str(), "saveremark=")) {
                                noremark_save_state = !section->Get_bool("saveremark");
                                mainMenu.get_item("noremark_savestate").check(noremark_save_state).refresh_item(mainMenu);
                            }
                            if (!strcasecmp(inputline.substr(0, 15).c_str(), "forceloadstate=")) {
                                force_load_state = section->Get_bool("forceloadstate");
                                mainMenu.get_item("force_loadstate").check(force_load_state).refresh_item(mainMenu);
                            }
						} else if (!strcasecmp(pvars[0].c_str(), "sdl")) {
							modifier = section->Get_string("clip_key_modifier");
							paste_speed = section->Get_int("clip_paste_speed");
							if (!strcasecmp(inputline.substr(0, 16).c_str(), "mouse_wheel_key=")) {
								wheel_key = section->Get_int("mouse_wheel_key");
								wheel_guest=wheel_key>0;
								if (wheel_key<0) wheel_key=-wheel_key;
								mainMenu.get_item("wheel_updown").check(wheel_key==1).refresh_item(mainMenu);
								mainMenu.get_item("wheel_leftright").check(wheel_key==2).refresh_item(mainMenu);
								mainMenu.get_item("wheel_pageupdown").check(wheel_key==3).refresh_item(mainMenu);
								mainMenu.get_item("wheel_none").check(wheel_key==0).refresh_item(mainMenu);
								mainMenu.get_item("wheel_guest").check(wheel_guest).refresh_item(mainMenu);
							}
							if (!strcasecmp(inputline.substr(0, 11).c_str(), "fullscreen=")) {
                                if (section->Get_bool("fullscreen")) {
                                    if (!GFX_IsFullscreen()) {GFX_LosingFocus();GFX_SwitchFullScreen();}
                                } else if (GFX_IsFullscreen()) {GFX_LosingFocus();GFX_SwitchFullScreen();}
                            }
                            if (!strcasecmp(inputline.substr(0, 7).c_str(), "output=")) {
                                bool toOutput(const char *output);
                                std::string GetDefaultOutput();
                                std::string output=section->Get_string("output");
                                if (output == "default") output=GetDefaultOutput();
                                GFX_LosingFocus();
                                toOutput(output.c_str());
                            }
#if defined(C_SDL2)
							if (!strcasecmp(inputline.substr(0, 16).c_str(), "mapperfile_sdl2=")) ReloadMapper(section,true);
#else
							if (!strcasecmp(inputline.substr(0, 16).c_str(), "mapperfile_sdl1=")) ReloadMapper(section,true);
#if !defined(HAIKU) && !defined(RISCOS)
							if (!strcasecmp(inputline.substr(0, 11).c_str(), "mapperfile=")) {
                                Prop_path* pp;
#if defined(C_SDL2)
                                pp = section->Get_path("mapperfile_sdl2");
#else
                                pp = section->Get_path("mapperfile_sdl1");
#endif
                                if (pp->realpath=="") ReloadMapper(section,true);
                            }
							if (!strcasecmp(inputline.substr(0, 13).c_str(), "usescancodes=")) {
								void setScanCode(Section_prop * section), loadScanCode(), MAPPER_Init();
								setScanCode(section);
								loadScanCode();
								GFX_LosingFocus();
								MAPPER_Init();
								load=true;
							}
#endif
#endif
                            if (!strcasecmp(inputline.substr(0, 8).c_str(), "display=")) {
                                void SetDisplayNumber(int display);
                                int GetNumScreen();
                                int numscreen = GetNumScreen();
                                const int display = section->Get_int("display");
                                if (display >= 0 && display <= numscreen)
                                    SetDisplayNumber(display);
                            }
                            if (!strcasecmp(inputline.substr(0, 15).c_str(), "windowposition=")) {
                                const char* windowposition = section->Get_string("windowposition");
                                int GetDisplayNumber(void);
                                int posx = -1, posy = -1;
                                if (windowposition && *windowposition) {
                                    char result[100];
                                    safe_strncpy(result, windowposition, sizeof(result));
                                    char* y = strchr(result, ',');
                                    if (y && *y) {
                                        *y = 0;
                                        posx = atoi(result);
                                        posy = atoi(y + 1);
                                    }
                                }
#if defined(C_SDL2)
                                SDL_Window* GFX_GetSDLWindow(void);
                                SDL_SetWindowTitle(GFX_GetSDLWindow(),"DOSBox-X");
                                if (posx < 0 || posy < 0) {
                                    SDL_DisplayMode dm;
                                    int w = 640,h = 480;
                                    SDL_GetWindowSize(GFX_GetSDLWindow(), &w, &h);
                                    if (SDL_GetDesktopDisplayMode(GetDisplayNumber()?GetDisplayNumber()-1:0,&dm) == 0) {
                                        posx = (dm.w - w)/2;
                                        posy = (dm.h - h)/2;
                                    }
                                }
                                if (GetDisplayNumber()>0) {
                                    int displays = SDL_GetNumVideoDisplays();
                                    SDL_Rect bound;
                                    for( int i = 1; i <= displays; i++ ) {
                                        bound = SDL_Rect();
                                        SDL_GetDisplayBounds(i-1, &bound);
                                        if (i == GetDisplayNumber()) {
                                            posx += bound.x;
                                            posy += bound.y;
                                            break;
                                        }
                                    }
                                }
                                SDL_SetWindowPosition(GFX_GetSDLWindow(), posx, posy);
#elif defined(WIN32)
                                RECT rect;
                                MONITORINFO info;
                                GetWindowRect(GetHWND(), &rect);
#if !defined(HX_DOS)
                                if (GetDisplayNumber()>0) {
                                    xyp xy={0};
                                    xy.x=-1;
                                    xy.y=-1;
                                    curscreen=0;
                                    BOOL CALLBACK EnumDispProc(HMONITOR hMon, HDC dcMon, RECT* pRcMon, LPARAM lParam);
                                    EnumDisplayMonitors(0, 0, EnumDispProc, reinterpret_cast<LPARAM>(&xy));
                                    HMONITOR monitor = MonitorFromRect(&monrect, MONITOR_DEFAULTTONEAREST);
                                    info.cbSize = sizeof(MONITORINFO);
                                    GetMonitorInfo(monitor, &info);
                                    if (posx >=0 && posy >=0) {
                                        posx+=info.rcMonitor.left;
                                        posy+=info.rcMonitor.top;
                                    } else {
                                        posx = info.rcMonitor.left+(info.rcMonitor.right-info.rcMonitor.left-(rect.right-rect.left))/2;
                                        posy = info.rcMonitor.top+(info.rcMonitor.bottom-info.rcMonitor.top-(rect.bottom-rect.top))/2;
                                    }
                                } else
#endif
                                if (posx < 0 && posy < 0) {
                                    posx = (GetSystemMetrics(SM_CXSCREEN)-(rect.right-rect.left))/2;
                                    posy = (GetSystemMetrics(SM_CYSCREEN)-(rect.bottom-rect.top))/2;
                                }
                                MoveWindow(GetHWND(), posx, posy, rect.right-rect.left, rect.bottom-rect.top, true);
#endif
                            }

#if defined(USE_TTF)
							resetFontSize();
#endif
						} else if (!strcasecmp(pvars[0].c_str(), "dos")) {
							mountwarning = section->Get_bool("mountwarning");
							if (!strcasecmp(inputline.substr(0, 4).c_str(), "lfn=")) {
								std::string lfn = section->Get_string("lfn");
								if (lfn=="true"||lfn=="1") enablelfn=1;
								else if (lfn=="false"||lfn=="0") enablelfn=0;
								else if (lfn=="autostart") enablelfn=-2;
								else enablelfn=-1;
								mainMenu.get_item("dos_lfn_auto").check(enablelfn==-1).refresh_item(mainMenu);
								mainMenu.get_item("dos_lfn_enable").check(enablelfn==1).refresh_item(mainMenu);
								mainMenu.get_item("dos_lfn_disable").check(enablelfn==0).refresh_item(mainMenu);
								uselfn = enablelfn==1 || ((enablelfn == -1 || enablelfn == -2) && (dos.version.major>6 || winrun));
							} else if (!strcasecmp(inputline.substr(0, 4).c_str(), "ver=")) {
								char *ver = (char *)section->Get_string("ver");
								if (!*ver) {
									dos.version.minor=0;
									dos.version.major=5;
									dos_ver_menu(false);
								} else if (set_ver(ver))
									dos_ver_menu(false);
							} else if (!strcasecmp(inputline.substr(0, 4).c_str(), "ems=")) {
								EMS_DoShutDown();
								EMS_Startup(NULL);
                                update_dos_ems_menu();
							} else if (!strcasecmp(inputline.substr(0, 32).c_str(), "shell configuration as commands=")) {
								enable_config_as_shell_commands = section->Get_bool("shell configuration as commands");
								mainMenu.get_item("shell_config_commands").check(enable_config_as_shell_commands).enable(true).refresh_item(mainMenu);
#if defined(WIN32) && !defined(HX_DOS)
                            } else if (!strcasecmp(inputline.substr(0, 13).c_str(), "automountall=")) {
                                if (section->Get_bool("automountall")) MountAllDrives(this);
                            } else if (!strcasecmp(inputline.substr(0, 9).c_str(), "dos clipboard api=")) {
                                clipboard_dosapi = section->Get_bool("dos clipboard api");          
							} else if (!strcasecmp(inputline.substr(0, 9).c_str(), "startcmd=")) {
								winautorun = section->Get_bool("startcmd");
								mainMenu.get_item("dos_win_autorun").check(winautorun).enable(true).refresh_item(mainMenu);
							} else if (!strcasecmp(inputline.substr(0, 10).c_str(), "startwait=")) {
								startwait = section->Get_bool("startwait");
								mainMenu.get_item("dos_win_wait").check(startwait).enable(true).refresh_item(mainMenu);
							} else if (!strcasecmp(inputline.substr(0, 11).c_str(), "startquiet=")) {
								startquiet = section->Get_bool("startquiet");
								mainMenu.get_item("dos_win_quiet").check(startquiet).enable(true).refresh_item(mainMenu);
#endif
                            }
						} else if (!strcasecmp(pvars[0].c_str(), "render")) {
                            void GFX_ForceRedrawScreen(void), ttf_reset(void), ttf_setlines(int cols, int lins), SetBlinkRate(Section_prop* section);
							if (!strcasecmp(inputline.substr(0, 9).c_str(), "ttf.font=")) {
#if defined(USE_TTF)
                                ttf_reset();
#endif
							} else if (!strcasecmp(inputline.substr(0, 9).c_str(), "ttf.lins=")||!strcasecmp(inputline.substr(0, 9).c_str(), "ttf.cols=")) {
#if defined(USE_TTF)
                                bool iscol=!strcasecmp(inputline.substr(0, 9).c_str(), "ttf.cols=");
                                if (iscol&&IS_PC98_ARCH)
                                    SetVal("render", "ttf.cols", "80");
                                else if (!CurMode)
                                    ;
                                else if (CurMode->type==M_TEXT || IS_PC98_ARCH)
                                    WriteOut("[2J");
                                else {
                                    reg_ax=(uint16_t)CurMode->mode;
                                    CALLBACK_RunRealInt(0x10);
                                }
                                lastset=iscol?2:1;
                                ttf_setlines(0, 0);
                                lastset=0;
#endif
							} else if (!strcasecmp(inputline.substr(0, 7).c_str(), "ttf.wp=")) {
#if defined(USE_TTF)
                                const char *wpstr=section->Get_string("ttf.wp");
                                wpType=wpVersion=0;
                                if (strlen(wpstr)>1) {
                                    if (!strncasecmp(wpstr, "WP", 2)) wpType=1;
                                    else if (!strncasecmp(wpstr, "WS", 2)) wpType=2;
                                    else if (!strncasecmp(wpstr, "XY", 3)) wpType=3;
                                    if (strlen(wpstr)>2&&wpstr[2]>='1'&&wpstr[2]<='9') wpVersion=wpstr[2]-'0';
                                }
                                mainMenu.get_item("ttf_wpno").check(!wpType).refresh_item(mainMenu);
                                mainMenu.get_item("ttf_wpwp").check(wpType==1).refresh_item(mainMenu);
                                mainMenu.get_item("ttf_wpws").check(wpType==2).refresh_item(mainMenu);
                                mainMenu.get_item("ttf_wpxy").check(wpType==3).refresh_item(mainMenu);
                                resetFontSize();
#endif
							} else if (!strcasecmp(inputline.substr(0, 9).c_str(), "ttf.wpbg=")) {
#if defined(USE_TTF)
                                wpBG = section->Get_int("ttf.wpbg");
                                resetFontSize();
#endif
							} else if (!strcasecmp(inputline.substr(0, 9).c_str(), "ttf.bold=")) {
#if defined(USE_TTF)
                                showbold = section->Get_bool("ttf.bold");
                                resetFontSize();
#endif
							} else if (!strcasecmp(inputline.substr(0, 11).c_str(), "ttf.italic=")) {
#if defined(USE_TTF)
                                showital = section->Get_bool("ttf.italic");
                                resetFontSize();
#endif
							} else if (!strcasecmp(inputline.substr(0, 14).c_str(), "ttf.underline=")) {
#if defined(USE_TTF)
                                showline = section->Get_bool("ttf.underline");
                                resetFontSize();
#endif
							} else if (!strcasecmp(inputline.substr(0, 14).c_str(), "ttf.strikeout=")) {
#if defined(USE_TTF)
                                showsout = section->Get_bool("ttf.strikeout");
                                resetFontSize();
#endif
							} else if (!strcasecmp(inputline.substr(0, 12).c_str(), "ttf.char512=")) {
#if defined(USE_TTF)
                                char512 = section->Get_bool("ttf.char512");
                                resetFontSize();
#endif
							} else if (!strcasecmp(inputline.substr(0, 11).c_str(), "ttf.blinkc=")) {
#if defined(USE_TTF)
                                SetBlinkRate(section);
#endif
							} else if (!strcasecmp(inputline.substr(0, 9).c_str(), "glshader=")) {
#if C_OPENGL
                                std::string LoadGLShader(Section_prop * section);
                                LoadGLShader(section);
                                GFX_ForceRedrawScreen();
#endif
							} else if (!strcasecmp(inputline.substr(0, 12).c_str(), "pixelshader="))
                                GFX_ForceRedrawScreen();
                        }
					}
				}
			} else WriteOut(MSG_Get("PROGRAM_CONFIG_VALUE_ERROR"),
				value.c_str(),pvars[1].c_str());
			return;
		}
		case P_WRITELANG: case P_WRITELANG2:
			// In secure mode don't allow a new languagefile to be created
			// Who knows which kind of file we would overwrite.
			if (securemode_check()) return;
			if (pvars.size() < 1) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_MISSINGPARAM"));
				return;
			}
			if (!MSG_Write(pvars[0].c_str())) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_FILE_ERROR"),pvars[0].c_str());
				return;
			}
			break;

		case P_SECURE:
			// Code for switching to secure mode
			control->SwitchToSecureMode();
			WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_ON"));
			return;

		default:
			E_Exit("bug");
			break;
		}
		first = false;
	}
	return;
}


static void CONFIG_ProgramStart(Program * * make) {
	*make=new CONFIG;
}

void PROGRAMS_DOS_Boot(Section *) {
	PROGRAMS_MakeFile("CONFIG.COM",CONFIG_ProgramStart);
}

/* FIXME: Rename the function to clarify it does not init programs, it inits the callback mechanism
 *        that program generation on drive Z: needs to tie a .COM executable to a callback */
void PROGRAMS_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("PROGRAMS_Init(): initializing Z: drive .COM stub and program management");

	/* Setup a special callback to start virtual programs */
	call_program=CALLBACK_Allocate();
	CALLBACK_Setup(call_program,&PROGRAMS_Handler,CB_RETF,"internal program");

    AddVMEventFunction(VM_EVENT_DOS_INIT_KERNEL_READY,AddVMEventFunctionFuncPair(PROGRAMS_DOS_Boot));

	// listconf
	MSG_Add("PROGRAM_CONFIG_NOCONFIGFILE","No config file loaded!\n");
	MSG_Add("PROGRAM_CONFIG_PRIMARY_CONF","Primary config file: \n%s\n");
	MSG_Add("PROGRAM_CONFIG_ADDITIONAL_CONF","Additional config files:\n");
	MSG_Add("PROGRAM_CONFIG_CONFDIR","DOSBox-X %s configuration directory: \n%s\n\n");
	MSG_Add("PROGRAM_CONFIG_WORKDIR","DOSBox-X's working directory: \n%s\n\n");
	
	// writeconf
	MSG_Add("PROGRAM_CONFIG_FILE_ERROR","\nCan't open file %s\n");
	MSG_Add("PROGRAM_CONFIG_FILE_WHICH","Writing config file %s\n");
	
	// help
	MSG_Add("PROGRAM_CONFIG_USAGE","The DOSBox-X command-line configuration utility. Supported options:\n"\
		"-wc (or -writeconf) without parameter: Writes to primary loaded config file.\n"\
		"-wc (or -writeconf) with filename: Writes file to the config directory.\n"\
		"-wl (or -writelang) with filename: Writes the current language strings.\n"\
		"-wcp [filename] Writes file to program directory (dosbox-x.conf or filename).\n"\
		"-wcd Writes to the default config file in the config directory.\n"\
		"-all Use this with -wc, -wcp, or -wcd to write ALL options to the config file.\n"\
		"-mod Use this with -wc, -wcp, or -wcd to write modified config options only.\n"\
		"-wcboot, -wcpboot, or -wcdboot will reboot DOSBox-X after writing the file.\n"\
		"-bootconf (or -bc) reboots with specified config file (or primary loaded file).\n"\
		"-norem Use this with -wc, -wcp, or -wcd to not write config option remarks.\n"\
		"-gui Starts DOSBox-X's graphical configuration tool.\n"
		"-l Lists DOSBox-X configuration parameters.\n"\
		"-h, -help, -? sections / sectionname / propertyname\n"\
		" Without parameters, displays this help screen. Add \"sections\" for a list of\n sections."\
		" For info about a specific section or property add its name behind.\n"\
		"-axclear Clears the [autoexec] section.\n"\
		"-axadd [line] Adds a line to the [autoexec] section.\n"\
		"-axtype Prints the content of the [autoexec] section.\n"\
		"-securemode Switches to secure mode where MOUNT, IMGMOUNT and BOOT will be\n"\
		" disabled as well as the ability to create config and language files.\n"\
		"-avistart starts AVI recording.\n"\
		"-avistop stops AVI recording.\n"\
		"-startmapper starts the keymapper.\n"\
		"-get \"section property\" returns the value of the property.\n"\
		"-set \"section property=value\" sets the value of the property.\n");
	MSG_Add("PROGRAM_CONFIG_HLP_PROPHLP","Purpose of property \"%s\" (contained in section \"%s\"):\n%s\n\nPossible Values: %s\nDefault value: %s\nCurrent value: %s\n");
	MSG_Add("PROGRAM_CONFIG_HLP_LINEHLP","Purpose of section \"%s\":\n%s\nCurrent value:\n%s\n");
	MSG_Add("PROGRAM_CONFIG_HLP_NOCHANGE","This property cannot be changed at runtime.\n");
	MSG_Add("PROGRAM_CONFIG_HLP_POSINT","positive integer"); 
	MSG_Add("PROGRAM_CONFIG_HLP_SECTHLP","Section %s contains the following properties:\n");				
	MSG_Add("PROGRAM_CONFIG_HLP_SECTLIST","DOSBox-X configuration contains the following sections:\n\n");

	MSG_Add("PROGRAM_CONFIG_SECURE_ON","Switched to secure mode.\n");
	MSG_Add("PROGRAM_CONFIG_SECURE_DISALLOW","This operation is not permitted in secure mode.\n");
	MSG_Add("PROGRAM_CONFIG_SECTION_ERROR","Section %s doesn't exist.\n");
	MSG_Add("PROGRAM_CONFIG_VALUE_ERROR","\"%s\" is not a valid value for property %s.\n");
	MSG_Add("PROGRAM_CONFIG_PROPERTY_ERROR","No such section or property.\n");
	MSG_Add("PROGRAM_CONFIG_PROPERTY_DUPLICATE","There may be other sections with the same property name.\n");
	MSG_Add("PROGRAM_CONFIG_NO_PROPERTY","There is no property %s in section %s.\n");
	MSG_Add("PROGRAM_CONFIG_SET_SYNTAX","Correct syntax: config -set \"section property=value\".\n");
	MSG_Add("PROGRAM_CONFIG_GET_SYNTAX","Correct syntax: config -get \"section property\".\n");
	MSG_Add("PROGRAM_CONFIG_PRINT_STARTUP","\nDOSBox-X was started with the following command line parameters:\n%s");
	MSG_Add("PROGRAM_CONFIG_MISSINGPARAM","Missing parameter.");
}
