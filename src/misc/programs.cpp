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

Bitu call_program;

extern int enablelfn, paste_speed;
extern const char *modifier;
extern bool dos_kernel_disabled, force_nocachedir, freesizecap, wpcolon;

/* This registers a file on the virtual drive and creates the correct structure for it*/

static Bit8u exe_block[]={
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
	Bit8u*		comdata;
	Bit32u		comsize;
	PROGRAMS_Main*	main;
};

static std::vector<InternalProgramEntry*> internal_progs;

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
	Bit32u size=sizeof(exe_block)+sizeof(Bit8u);
	InternalProgramEntry *ipe;
	Bit8u *comdata;
	Bit8u index;

	/* Copy save the pointer in the vector and save it's index */
	if (internal_progs.size()>255) E_Exit("PROGRAMS_MakeFile program size too large (%d)",static_cast<int>(internal_progs.size()));

	index = (Bit8u)internal_progs.size();
	comdata = (Bit8u *)malloc(32); //MEM LEAK
	memcpy(comdata,&exe_block,sizeof(exe_block));
	memcpy(&comdata[sizeof(exe_block)],&index,sizeof(index));
	comdata[CB_POS]=(Bit8u)(call_program&0xff);
	comdata[CB_POS+1]=(Bit8u)((call_program>>8)&0xff);

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
	Bitu size=sizeof(Bit8u);
	Bit8u index;
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

	if (/*control->SecureMode() ||*/ cmd->Get_arglength() > 100) {	
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

	Bit16u size = (Bit16u)strlen(buf);
	dos.internal_output=true;
	for(Bit16u i = 0; i < size;i++) {
		Bit8u out;Bit16u s=1;
		if (buf[i] == 0xA && last_written_character != 0xD) {
			out = 0xD;DOS_WriteFile(STDOUT,&out,&s);
		}
		last_written_character = (char)(out = (Bit8u)buf[i]);
		DOS_WriteFile(STDOUT,&out,&s);
	}
	dos.internal_output=false;
	
//	DOS_WriteFile(STDOUT,(Bit8u *)buf,&size);
}

void Program::WriteOut_NoParsing(const char * format) {
	Bit16u size = (Bit16u)strlen(format);
	char const* buf = format;
	dos.internal_output=true;
	for(Bit16u i = 0; i < size;i++) {
		Bit8u out;Bit16u s=1;
		if (buf[i] == 0xA && last_written_character != 0xD) {
			out = 0xD;DOS_WriteFile(STDOUT,&out,&s);
		}
		last_written_character = (char)(out = (Bit8u)buf[i]);
		DOS_WriteFile(STDOUT,&out,&s);
	}
	dos.internal_output=false;

//	DOS_WriteFile(STDOUT,(Bit8u *)format,&size);
}

static bool LocateEnvironmentBlock(PhysPt &env_base,PhysPt &env_fence,Bitu env_seg) {
	if (env_seg == 0) {
		/* The DOS program might have freed it's environment block perhaps. */
		return false;
	}

	DOS_MCB env_mcb((Bit16u)(env_seg-1)); /* read the environment block's MCB to determine how large it is */
	env_base = PhysMake((Bit16u)env_seg,0);
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
		for (const char *s=bigentry.c_str();*s != 0;) mem_writeb(env_scan++,(Bit8u)(*s++));
		mem_writeb(env_scan++,'=');

		assert(env_scan < env_fence);
		for (const char *s=new_string;*s != 0;) mem_writeb(env_scan++,(Bit8u)(*s++));
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
	
	void writeconf(std::string name, bool configdir,bool everything) {
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
		if (!control->PrintConfig(name.c_str(),everything)) {
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

void CONFIG::Run(void) {
	static const char* const params[] = {
		"-r", "-wcp", "-wcd", "-wc", "-writeconf", "-l", "-rmconf",
		"-h", "-help", "-?", "-axclear", "-axadd", "-axtype", "-get", "-set",
		"-writelang", "-wl", "-securemode", "-all", "-errtest", NULL };
	enum prs {
		P_NOMATCH, P_NOPARAMS, // fixed return values for GetParameterFromList
		P_RESTART,
		P_WRITECONF_PORTABLE, P_WRITECONF_DEFAULT, P_WRITECONF, P_WRITECONF2,
		P_LISTCONF,	P_KILLCONF,
		P_HELP, P_HELP2, P_HELP3,
		P_AUTOEXEC_CLEAR, P_AUTOEXEC_ADD, P_AUTOEXEC_TYPE,
		P_GETPROP, P_SETPROP,
		P_WRITELANG, P_WRITELANG2,
		P_SECURE, P_ALL, P_ERRTEST
	} presult = P_NOMATCH;

	bool all = false;
	bool first = true;
	std::vector<std::string> pvars;
	if (cmd->FindExist("-all", true)) all = true;
	// Loop through the passed parameters
	while(presult != P_NOPARAMS) {
		presult = (enum prs)cmd->GetParameterFromList(params, pvars);
		switch(presult) {
	
		case P_ALL:
			all = true;
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
		case P_WRITECONF: case P_WRITECONF2:
			if (securemode_check()) return;
			if (pvars.size() > 1) return;
			else if (pvars.size() == 1) {
				// write config to specific file, except if it is an absolute path
				writeconf(pvars[0], !Cross::IsPathAbsolute(pvars[0]), all);
			} else {
				// -wc without parameter: write primary config file
				if (control->configfiles.size()) writeconf(control->configfiles[0], false, all);
				else WriteOut(MSG_Get("PROGRAM_CONFIG_NOCONFIGFILE"));
			}
			break;
		case P_WRITECONF_DEFAULT: {
			// write to /userdir/dosbox-x-0.xx.conf
			if (securemode_check()) return;
			if (pvars.size() > 0) return;
			std::string confname;
			Cross::GetPlatformConfigName(confname);
			writeconf(confname, true, all);
			break;
		}
		case P_WRITECONF_PORTABLE:
			if (securemode_check()) return;
			if (pvars.size() > 1) return;
			else if (pvars.size() == 1) {
				// write config to startup directory
				writeconf(pvars[0], false, all);
			} else {
				// -wcp without parameter: write dosbox-x.conf to startup directory
				writeconf(std::string("dosbox-x.conf"), false, all);
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
					WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"));
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
						WriteOut("%s=%s\n", p->propname.c_str(),
							p->GetValue().ToString().c_str());
					}
					if (!strcasecmp(pvars[0].c_str(), "config")) {
						const char * extra = const_cast<char*>(psec->data.c_str());
						if (extra&&strlen(extra)) {
							std::istringstream in(extra);
							char linestr[CROSS_LEN+1], cmdstr[CROSS_LEN], valstr[CROSS_LEN];
							char *cmd=cmdstr, *val=valstr, *lin=linestr, *p;
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
									if (!strncmp(cmd, "set ", 4)||!strcmp(cmd, "install")||!strcmp(cmd, "installhigh")||!strcmp(cmd, "device")||!strcmp(cmd, "devicehigh"))
										WriteOut("%s=%s\n", cmd, val);
								}
							}
						}
					}
				} else {
					// no: maybe it's a property?
					sec = control->GetSectionFromProperty(pvars[0].c_str());
					if (!sec) {
						WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"));
						return;
					}
					// it's a property name
					std::string val = sec->GetPropValue(pvars[0].c_str());
					WriteOut("%s",val.c_str());
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
							char *cmd=cmdstr, *val=valstr, *lin=linestr, *p;
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
				WriteOut("%s",val.c_str());
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
			if (value.empty() ) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_SET_SYNTAX"));
				return;
			}
			std::string inputline = pvars[1] + "=" + value;
			
			bool change_success = tsec->HandleInputline(inputline.c_str());
			if (change_success) {
				if (!strcasecmp(pvars[0].c_str(), "dosbox")||!strcasecmp(pvars[0].c_str(), "sdl")||!strcasecmp(pvars[0].c_str(), "dos")) {
					Section_prop *section = static_cast<Section_prop *>(control->GetSection(pvars[0].c_str()));
					if (section != NULL) {
						if (!strcasecmp(pvars[0].c_str(), "dosbox")) {
							force_nocachedir = section->Get_bool("nocachedir");
							freesizecap = section->Get_bool("freesizecap");
							wpcolon = section->Get_bool("leading colon write protect image");
						} else if (!strcasecmp(pvars[0].c_str(), "sdl")) {
							modifier = section->Get_string("clip_key_modifier");
							paste_speed = section->Get_int("clip_paste_speed");
						} else if (!strcasecmp(pvars[0].c_str(), "dos")) {
							if (!strcasecmp(inputline.substr(0, 4).c_str(), "lfn=")) {
								if (!strcmp(section->Get_string("lfn"), "true")) enablelfn=1;
								else if (!strcmp(section->Get_string("lfn"), "false")) enablelfn=0;
								else if (!strcmp(section->Get_string("lfn"), "autostart")) enablelfn=-2;
								else enablelfn=-1;
								uselfn = enablelfn==1 || ((enablelfn == -1 || enablelfn == -2) && dos.version.major>6);
							} else if (!strcasecmp(inputline.substr(0, 4).c_str(), "ver=")) {
								std::string ver = section->Get_string("ver");
								if (!ver.empty()) {
									const char *s = ver.c_str();
									if (isdigit(*s)) {
										dos.version.minor=0;
										dos.version.major=(int)strtoul(s,(char**)(&s),10);
										if (*s == '.' || *s == ' ') {
											s++;
											if (isdigit(*s))
												dos.version.minor=(*(s-1)=='.'&&strlen(s)==1?10:1)*(int)strtoul(s,(char**)(&s),10);
										}
										uselfn = enablelfn==1 || ((enablelfn == -1 || enablelfn == -2) && dos.version.major>6);
									}
								}
							}
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
	
	// writeconf
	MSG_Add("PROGRAM_CONFIG_FILE_ERROR","\nCan't open file %s\n");
	MSG_Add("PROGRAM_CONFIG_FILE_WHICH","Writing config file %s\n");
	
	// help
	MSG_Add("PROGRAM_CONFIG_USAGE","The DOSBox-X config tool. Supported options:\n\n"\
		"-wc (or -writeconf) without parameter: Writes to primary loaded config file.\n"\
		"-wc (or -writeconf) with filename: Writes file to the config directory.\n"\
		"-wl (or -writelang) with filename: Writes the current language strings.\n"\
		"-wcp [filename] Writes config file to the program directory (dosbox-x.conf\n or the specified filename).\n"\
		"-wcd Writes to the default config file in the config directory.\n"\
		"-all Use this with -wc, -wcp, or -wcd to write ALL options to the config file.\n"\
		"-l Lists DOSBox-X configuration parameters.\n"\
		"-h, -help, -? sections / sectionname / propertyname\n"\
		" Without parameters, displays this help screen. Add \"sections\" for a list of\n sections."\
		" For info about a specific section or property add its name behind.\n"\
		"-axclear Clears the [autoexec] section.\n"\
		"-axadd [line] Adds a line to the [autoexec] section.\n"\
		"-axtype Prints the content of the [autoexec] section.\n"\
		"-securemode\n"\
        " Switches to secure mode where MOUNT, IMGMOUNT and BOOT will be disabled\n"\
        " as well as the ability to create config and language files.\n"\
		"-get \"section property\" returns the value of the property.\n"\
		"-set \"section property=value\" sets the value of the property.\n" );
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
	MSG_Add("PROGRAM_CONFIG_NO_PROPERTY","There is no property %s in section %s.\n");
	MSG_Add("PROGRAM_CONFIG_SET_SYNTAX","Correct syntax: config -set \"section property=value\".\n");
	MSG_Add("PROGRAM_CONFIG_GET_SYNTAX","Correct syntax: config -get \"section property\".\n");
	MSG_Add("PROGRAM_CONFIG_PRINT_STARTUP","\nDOSBox-X was started with the following command line parameters:\n%s");
	MSG_Add("PROGRAM_CONFIG_MISSINGPARAM","Missing parameter.");
}
