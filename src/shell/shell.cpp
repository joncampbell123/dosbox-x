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

#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <gtest/gtest.h>

#include "dosbox.h"
#include "logging.h"
#include "regs.h"
#include "control.h"
#include "shell.h"
#include "menu.h"
#include "cpu.h"
#include "callback.h"
#include "dos_inc.h"
#include "support.h"
#include "mapper.h"
#include "render.h"
#include "jfont.h"
#include "../dos/drives.h"
#include "../ints/int10.h"
#include <unistd.h>
#include <time.h>
#include <string>
#include <sstream>
#include <vector>
#if defined(WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif
#include "build_timestamp.h"

extern bool shell_keyboard_flush;
extern bool dos_shell_running_program, mountwarning, winautorun;
extern bool startcmd, startwait, startquiet, internal_program;
extern bool addovl, addipx, addne2k, enableime, showdbcs;
extern bool halfwidthkana, force_conversion, gbk;
extern const char* RunningProgram;
extern int enablelfn, msgcodepage, lastmsgcp;
extern uint16_t countryNo;
extern unsigned int dosbox_shell_env_size;
bool outcon = true, usecon = true, pipetmpdev = true;
bool shellrun = false, prepared = false, testerr = false;

uint16_t shell_psp = 0;
Bitu call_int2e = 0;

std::string GetDOSBoxXPath(bool withexe=false);
const char* DOS_GetLoadedLayout(void);
int Reflect_Menu(void);
void SetIMPosition(void);
void SetKEYBCP();
void initRand();
void initcodepagefont(void);
void runMount(const char *str);
void ResolvePath(std::string& in);
void DOS_SetCountry(uint16_t countryNo);
void SwitchLanguage(int oldcp, int newcp, bool confirm);
void CALLBACK_DeAllocate(Bitu in), DOSBox_ConsolePauseWait();
void GFX_SetTitle(int32_t cycles, int frameskip, Bits timing, bool paused);
bool isDBCSCP(), InitCodePage(), isKanji1(uint8_t chr), shiftjis_lead_byte(int c), sdl_wait_on_error();

Bitu call_shellstop = 0;
/* Larger scope so shell_del autoexec can use it to
 * remove things from the environment */
DOS_Shell * first_shell = 0; 

static Bitu shellstop_handler(void) {
	return CBRET_STOP;
}

void SHELL_ProgramStart(Program * * make) {
	*make = new DOS_Shell;
}
//Repeat it with the correct type, could do it in the function below, but this way it should be 
//clear that if the above function is changed, this function might need a change as well.
static void SHELL_ProgramStart_First_shell(DOS_Shell * * make) {
	*make = new DOS_Shell;
}

bool i4dos=false;
char i4dos_data[CONFIG_SIZE] = { 0 };
char config_data[CONFIG_SIZE] = { 0 };
char autoexec_data[AUTOEXEC_SIZE] = { 0 };
static std::list<std::string> autoexec_strings;
typedef std::list<std::string>::iterator auto_it;

void VFILE_Remove(const char *name,const char *dir="");
void runRescan(const char *str), DOSBox_SetSysMenu(void);
int toSetCodePage(DOS_Shell *shell, int newCP, int opt);

#if defined(WIN32)
void MountAllDrives(bool quiet) {
    char str[100];
    uint16_t n = 0;
    uint32_t drives = GetLogicalDrives();
    char name[4]="A:\\";
    for (int i=0; i<25; i++) {
        if ((drives & (1<<i)) && !Drives[i])
        {
            name[0]='A'+i;
            int type=GetDriveType(name);
            if (type!=DRIVE_NO_ROOT_DIR) {
                if (!quiet) {
                    sprintf(str, "Mounting %c: => %s..\r\n", name[0], name);
                    n = (uint16_t)strlen(str);
                    DOS_WriteFile(STDOUT,(uint8_t *)str,&n);
                }
                char mountstring[DOS_PATHLENGTH+CROSS_LEN+20];
                name[2]=' ';
                strcpy(mountstring,name);
                name[2]='\\';
                strcat(mountstring,name);
                strcat(mountstring," -Q");
                runMount(mountstring);
                if (!Drives[i] && !quiet) {
                    sprintf(str, "Drive %c: failed to mount.\r\n",name[0]);
                    n = (uint16_t)strlen(str);
                    DOS_WriteFile(STDOUT,(uint8_t *)str,&n);
                } else if(mountwarning && !quiet && type==DRIVE_FIXED && (strcasecmp(name,"C:\\")==0)) {
                    strcpy(str, MSG_Get("PROGRAM_MOUNT_WARNING_WIN"));
                    if (strlen(str)>2&&str[strlen(str)-1]=='\n'&&str[strlen(str)-2]!='\r') {
                        str[strlen(str)-1]=0;
                        strcat(str, "\r\n");
                    }
                    n = (uint16_t)strlen(str);
                    DOS_WriteFile(STDOUT,(uint8_t *)str,&n);
                }
            }
        }
    }
}
#endif

void AutoexecObject::Install(const std::string &in) {
	if(GCC_UNLIKELY(installed)) E_Exit("autoexec: already created %s",buf.c_str());
	installed = true;
	buf = in;
	autoexec_strings.push_back(buf);
	this->CreateAutoexec();

	//autoexec.bat is normally created AUTOEXEC_Init.
	//But if we are already running (first_shell)
	//we have to update the environment to display changes

	if(first_shell)	{
		//create a copy as the string will be modified
		std::string::size_type n = buf.size();
		char* buf2 = new char[n + 1];
		safe_strncpy(buf2, buf.c_str(), n + 1);
		if((strncasecmp(buf2,"set ",4) == 0) && (strlen(buf2) > 4)){
			char* after_set = buf2 + 4;//move to variable that is being set
			char* test2 = strpbrk(after_set,"=");
			if(!test2) {first_shell->SetEnv(after_set,"");return;}
			*test2++ = 0;
			//If the shell is running/exists update the environment
			first_shell->SetEnv(after_set,test2);
		}
		delete [] buf2;
	}
}

void AutoexecObject::InstallBefore(const std::string &in) {
	if(GCC_UNLIKELY(installed)) E_Exit("autoexec: already created %s",buf.c_str());
	installed = true;
	buf = in;
	autoexec_strings.push_front(buf);
	this->CreateAutoexec();
}

void AutoexecObject::CreateAutoexec(void) {
	/* Remove old autoexec.bat if the shell exists */
	if(first_shell)	VFILE_Remove("AUTOEXEC.BAT");

	//Create a new autoexec.bat
	autoexec_data[0] = 0;
	size_t auto_len;
	for(auto_it it = autoexec_strings.begin(); it != autoexec_strings.end(); ++it) {

		std::string linecopy = *it;
		std::string::size_type offset = 0;
		//Lets have \r\n as line ends in autoexec.bat.
		while(offset < linecopy.length()) {
			std::string::size_type  n = linecopy.find("\n",offset);
			if ( n == std::string::npos ) break;
			std::string::size_type rn = linecopy.find("\r\n",offset);
			if ( rn != std::string::npos && rn + 1 == n) {offset = n + 1; continue;}
			// \n found without matching \r
			linecopy.replace(n,1,"\r\n");
			offset = n + 2;
		}

		auto_len = strlen(autoexec_data);
		if ((auto_len+linecopy.length() + 3) > AUTOEXEC_SIZE) {
			E_Exit("SYSTEM:Autoexec.bat file overflow");
		}
		sprintf((autoexec_data + auto_len),"%s\r\n",linecopy.c_str());
	}
	if (first_shell) {
        internal_program = true;
        VFILE_Register("AUTOEXEC.BAT",(uint8_t *)autoexec_data,(uint32_t)strlen(autoexec_data));
        internal_program = false;
    }
}

void AutoexecObject::Uninstall() {
	if(!installed) return;

	// Remove the line from the autoexecbuffer and update environment
	for(auto_it it = autoexec_strings.begin(); it != autoexec_strings.end(); ) {
		if ((*it) == buf) {
			std::string::size_type n = buf.size();
			char* buf2 = new char[n + 1];
			safe_strncpy(buf2, buf.c_str(), n + 1);
			bool stringset = false;
			// If it's a environment variable remove it from there as well
			if ((strncasecmp(buf2,"set ",4) == 0) && (strlen(buf2) > 4)){
				char* after_set = buf2 + 4;//move to variable that is being set
				char* test2 = strpbrk(after_set,"=");
				if (!test2) {
					delete [] buf2;
					continue;
				}
				*test2 = 0;
				stringset = true;
				//If the shell is running/exists update the environment
				if (first_shell) first_shell->SetEnv(after_set,"");
			}
			delete [] buf2;
			if (stringset && first_shell && first_shell->bf && first_shell->bf->filename.find("AUTOEXEC.BAT") != std::string::npos) {
				//Replace entry with spaces if it is a set and from autoexec.bat, as else the location counter will be off.
				*it = buf.assign(buf.size(),' ');
				++it;
			} else {
				it = autoexec_strings.erase(it);
			}
		} else ++it;
	}
	installed=false;
	this->CreateAutoexec();
}

AutoexecObject::~AutoexecObject(){
	Uninstall();
}

DOS_Shell::~DOS_Shell() {
	if (bf != NULL) delete bf; /* free batch file */
}

DOS_Shell::DOS_Shell():Program(){
	input_handle=STDIN;
	echo=true;
	exit=false;
    perm = false;
	bf=0;
	call=false;
	exec=false;
	lfnfor = uselfn;
    input_eof=false;
    completion_index = 0;
}

Bitu DOS_Shell::GetRedirection(char *s, char **ifn, char **ofn, char **toc,bool * append) {

	char * lr=s;
	char * lw=s;
	char ch;
	Bitu num=0;
	bool quote = false;
	bool lead1 = false, lead2 = false;
	char* t;
	int q;

	while ( (ch=*lr++) ) {
		if(quote && ch != '"') { /* don't parse redirection within quotes. Not perfect yet. Escaped quotes will mess the count up */
			*lw++ = ch;
			continue;
		}
        if (lead1) {
            lead1=false;
            if (ch=='|') {
                *lw++=ch;
                continue;
            }
        } else if ((IS_PC98_ARCH && shiftjis_lead_byte(ch)) || (isDBCSCP() && !((dos.loaded_codepage == 936 || IS_PDOSV) && !gbk) && isKanji1(ch))) lead1 = true;
		switch (ch) {
		case '"':
			quote = !quote;
			break;
		case '>':
			*append = ((*lr) == '>');
			if (*append)
				lr++;
			lr = ltrim(lr);
			if (*ofn)
				free(*ofn);
			*ofn = lr;
			q = 0;
			lead2 = false;
			while (*lr && (q/2*2!=q || *lr != ' ') && *lr != '<' && !(!lead2 && *lr == '|')) {
                if (lead2)
                    lead2 = false;
                else if ((IS_PC98_ARCH && shiftjis_lead_byte(*lr&0xff)) || (isDBCSCP() && !((dos.loaded_codepage == 936 || IS_PDOSV) && !gbk) && isKanji1(*lr&0xff)))
                    lead2 = true;
				if (*lr=='"')
					q++;
				lr++;
			}
			// if it ends on a : => remove it.
			if ((*ofn != lr) && (lr[-1] == ':'))
				lr[-1] = 0;
			t = (char*)malloc(lr-*ofn+1);
			safe_strncpy(t, *ofn, lr-*ofn+1);
			*ofn = t;
			continue;
		case '<':
			if (*ifn)
				free(*ifn);
			lr = ltrim(lr);
			*ifn = lr;
			q = 0;
			lead2 = false;
			while (*lr && (q/2*2!=q || *lr != ' ') && *lr != '>' && !(!lead2 && *lr == '|')) {
                if (lead2)
                    lead2 = false;
                else if ((IS_PC98_ARCH && shiftjis_lead_byte(*lr&0xff)) || (isDBCSCP() && !((dos.loaded_codepage == 936 || IS_PDOSV) && !gbk) && isKanji1(*lr&0xff)))
                    lead2 = true;
				if (*lr=='"')
					q++;
				lr++;
			}
			if ((*ifn != lr) && (lr[-1] == ':'))
				lr[-1] = 0;
			t = (char*)malloc(lr-*ifn+1);
			safe_strncpy(t, *ifn, lr-*ifn+1);
			*ifn = t;
			continue;
		case '|':
			num++;
			if (*toc)
				free(*toc);
			lr = ltrim(lr);
			*toc = lr;
			while (*lr)
				lr++;
			t = (char*)malloc(lr-*toc+1);
			safe_strncpy(t, *toc, lr-*toc+1);
			*toc = t;
			continue;
		}
		*lw++=ch;
	}
	*lw=0;
	return num;
}	

class device_TMP : public DOS_Device {
public:
    std::string str = "";
    unsigned long long curpos = 0;
	device_TMP(char *name) { SetName(name); };
	virtual bool Read(uint8_t * data,uint16_t * size) {
        int i;
        for (i=0; i<*size; i++) {
            if (curpos+i>=str.size()) break;
            *data++ = str.substr(curpos++, 1)[0];
        }
		*size = i;
		return true;
	}
	virtual bool Write(const uint8_t * data,uint16_t * size) {
        for (int i=0; i<*size; i++) str += std::string(1, data[i]);
		return true;
	}
	virtual bool Seek(uint32_t * pos,uint32_t type) {
		switch (type) {
            case 0:
                curpos = *pos;
                break;
            case 1:
                curpos = curpos+*pos;
                break;
            case 2:
                curpos = str.size()+*pos;
                break;
            default:
                DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
                return false;
		}
		if (curpos > str.size()) curpos = str.size();
		else if (curpos < 0) curpos = 0;
		return true;
	}
	virtual bool Close() { return true; }
	virtual uint16_t GetInformation(void) { return (strcmp(RunningProgram, "WCLIP") ? DeviceInfoFlags::Device : 0) | DeviceInfoFlags::EofOnInput; }
	virtual bool ReadFromControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode) { (void)bufptr; (void)size; (void)retcode; return false; }
	virtual bool WriteToControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode) { (void)bufptr; (void)size; (void)retcode; return false; }
};

void DOS_Shell::ParseLine(char * line) {
	LOG(LOG_EXEC,LOG_DEBUG)("Parsing command line: %s",line);
	/* Check for a leading @ */
 	if (line[0] == '@') line[0] = ' ';
	line = trim(line);

	/* Do redirection and pipe checks */
	
	char * in  = 0;
	char * out = 0;
	char * toc = 0;

	uint16_t dummy,dummy2;
	uint32_t bigdummy = 0;
	bool append;
	bool normalstdin  = false;	/* whether stdin/out are open on start. */
	bool normalstdout = false;	/* Bug: Assumed is they are "con"      */
	
    GetRedirection(line, &in, &out, &toc, &append);
	if (in || out || toc) {
		normalstdin  = (psp->GetFileHandle(0) != 0xff); 
		normalstdout = (psp->GetFileHandle(1) != 0xff); 
	}
	if (in) {
		if(DOS_OpenFile(in,OPEN_READ,&dummy)) {	//Test if file exists
			DOS_CloseFile(dummy);
			LOG_MSG("SHELL:Redirect input from %s",in);
			if(normalstdin) DOS_CloseFile(0);	//Close stdin
			DOS_OpenFile(in,OPEN_READ,&dummy);	//Open new stdin
		} else {
			WriteOut(!*in?"File open error\n":(dos.errorcode==DOSERR_ACCESS_DENIED?MSG_Get("SHELL_CMD_FILE_ACCESS_DENIED"):"File open error - %s\n"), in);
			in = 0;
			return;
		}
	}
	bool fail=false;
	char pipetmp[270];
	uint16_t fattr;
	if (toc) {
		initRand();
		std::string line;
		if (!GetEnvStr("TEMP",line)&&!GetEnvStr("TMP",line))
			sprintf(pipetmp, "pipe%d.tmp", rand()%10000);
		else {
			std::string::size_type idx = line.find('=');
			std::string temp=line.substr(idx +1 , std::string::npos);
			if (DOS_GetFileAttr(temp.c_str(), &fattr) && fattr&DOS_ATTR_DIRECTORY)
				sprintf(pipetmp, "%s\\pipe%d.tmp", temp.c_str(), rand()%10000);
			else
				sprintf(pipetmp, "pipe%d.tmp", rand()%10000);
		}
	}
	DOS_Device *tmpdev = NULL;
	if (out||toc) {
		if (out&&toc)
			WriteOut(!*out?"Duplicate redirection\n":"Duplicate redirection - %s\n", out);
		LOG_MSG("SHELL:Redirect output to %s",toc?pipetmp:out);
		if(normalstdout) DOS_CloseFile(1);
		if(!normalstdin && !in) DOS_OpenFile("con",OPEN_READWRITE,&dummy);
		bool status = true;
		/* Create if not exist. Open if exist. Both in read/write mode */
		if(!toc&&append) {
			if (DOS_GetFileAttr(out, &fattr) && fattr&DOS_ATTR_READ_ONLY) {
				DOS_SetError(DOSERR_ACCESS_DENIED);
				status = false;
			} else if( (status = DOS_OpenFile(out,OPEN_READWRITE,&dummy)) ) {
				 DOS_SeekFile(1,&bigdummy,DOS_SEEK_END);
			} else {
				status = DOS_CreateFile(out,DOS_ATTR_ARCHIVE,&dummy);	//Create if not exists.
			}
		} else if (!toc&&DOS_GetFileAttr(out, &fattr) && fattr&DOS_ATTR_READ_ONLY) {
			DOS_SetError(DOSERR_ACCESS_DENIED);
			status = false;
		} else {
            bool device=DOS_FindDevice(pipetmp)!=DOS_DEVICES;
			if (toc&&!device&&DOS_FindFirst(pipetmp, ~DOS_ATTR_VOLUME)&&!DOS_UnlinkFile(pipetmp))
				fail=true;
			status = device?false:DOS_OpenFileExtended(toc&&!fail?pipetmp:out,OPEN_READWRITE,DOS_ATTR_ARCHIVE,0x12,&dummy,&dummy2);
			if (toc&&(fail||!status)&&!strchr(pipetmp,'\\')) {
                Overlay_Drive *da = Drives[0] ? (Overlay_Drive *)Drives[0] : NULL, *dc = Drives[2] ? (Overlay_Drive *)Drives[2] : NULL;
                if ((Drives[0]&&!Drives[0]->readonly&&!(da&&da->ovlreadonly))||(Drives[2]&&!Drives[2]->readonly&&!(dc&&dc->ovlreadonly))) {
                    int len = (int)strlen(pipetmp);
                    if (len > 266) {
                        len = 266;
                        pipetmp[len] = 0;
                    }
                    for (int i = len; i >= 0; i--)
                        pipetmp[i + 3] = pipetmp[i];
                    pipetmp[0] = Drives[2]?'c':'a';
                    pipetmp[1] = ':';
                    pipetmp[2] = '\\';
                    fail=false;
                } else if (!tmpdev && pipetmpdev) {
                    char *p=strchr(pipetmp, '.');
                    if (p) *p = 0;
                    tmpdev = new device_TMP(pipetmp);
                    if (p) *p = '.';
                    if (tmpdev) {
                        DOS_AddDevice(tmpdev);
                        fail = false;
                    }
                } else
                    fail=true;
                if (!tmpdev && DOS_FindFirst(pipetmp, ~DOS_ATTR_VOLUME) && !DOS_UnlinkFile(pipetmp))
                    fail=true;
                else
                    status = DOS_OpenFileExtended(pipetmp, OPEN_READWRITE, DOS_ATTR_ARCHIVE, 0x12, &dummy, &dummy2);
            }
		}
		if(!status && normalstdout) {
			DOS_OpenFile("con", OPEN_READWRITE, &dummy);							// Read only file, open con again
			if (!toc) {
				WriteOut(!*out?"File creation error\n":(dos.errorcode==DOSERR_ACCESS_DENIED?MSG_Get("SHELL_CMD_FILE_ACCESS_DENIED"):"File creation error - %s\n"), out);
				DOS_CloseFile(1);
				DOS_OpenFile("nul", OPEN_READWRITE, &dummy);
			}
		}
		if(!normalstdin && !in) DOS_CloseFile(0);
	}
	/* Run the actual command */

	if (this == first_shell) dos_shell_running_program = true;
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
	Reflect_Menu();
#endif
	if (toc||(!toc&&((out&&DOS_FindDevice(out)!=DOS_FindDevice("con"))))) outcon=false;
	if (toc||(!toc&&((out&&DOS_FindDevice(out)!=DOS_FindDevice("con"))||(in&&DOS_FindDevice(in)!=DOS_FindDevice("con"))))) usecon=false;

	DoCommand(line);

	if (this == first_shell) dos_shell_running_program = false;
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
	Reflect_Menu();
#endif

	/* Restore handles */
	if(in) {
		DOS_CloseFile(0);
		if(normalstdin) DOS_OpenFile("con",OPEN_READWRITE,&dummy);
		free(in);
	}
	if(out||toc) {
		DOS_CloseFile(1);
		if(!normalstdin) DOS_OpenFile("con",OPEN_READWRITE,&dummy);
		if(normalstdout) DOS_OpenFile("con",OPEN_READWRITE,&dummy);
		if(!normalstdin) DOS_CloseFile(0);
		if (out) free(out);
	}
	if (toc) {
		if (!fail&&DOS_OpenFile(pipetmp, OPEN_READ, &dummy))					// Test if file can be opened for reading
			{
			DOS_CloseFile(dummy);
			if (normalstdin)
				DOS_CloseFile(0);												// Close stdin
			DOS_OpenFile(pipetmp, OPEN_READ, &dummy);							// Open new stdin
			ParseLine(toc);
			DOS_CloseFile(0);
			if (normalstdin)
				DOS_OpenFile("con", OPEN_READWRITE, &dummy);
			}
		else
			WriteOut("\nFailed to create/open a temporary file for piping. Check the %%TEMP%% variable.\n");
		free(toc);
		if (tmpdev) {
			DOS_DelDevice(tmpdev);
			tmpdev = NULL;
		} else if (DOS_FindFirst(pipetmp, ~DOS_ATTR_VOLUME)) DOS_UnlinkFile(pipetmp);
	}
	outcon=usecon=true;
}

void DOS_Shell::RunInternal(void) {
	char input_line[CMD_MAXLINE] = {0};
	while (bf) {
		if (bf->ReadLine(input_line)) {
			if (echo) {
				if (input_line[0] != '@') {
					ShowPrompt();
					WriteOut_NoParsing(input_line);
					WriteOut_NoParsing("\n");
				}
			}
			ParseLine(input_line);
			if (echo) WriteOut_NoParsing("\n");
		}
	}
}

bool ansiinstalled = true, ANSI_SYS_installed();
extern void ReadCharAttr(uint16_t col,uint16_t row,uint8_t page,uint16_t * result);
bool is_ANSI_installed(Program *shell) {
    if (ANSI_SYS_installed()) return true;
    uint16_t oldax=reg_ax;
    if (CurMode->type == M_TEXT) {
        shell->WriteOut("-\033[2J=+");
        uint8_t page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
        uint16_t result1, result2;
        ReadCharAttr(0,0,page,&result1);
        ReadCharAttr(1,0,page,&result2);
        bool installed = (uint8_t)result1=='=' && (uint8_t)result2=='+';
        if (installed) {
            shell->WriteOut("\033[2J");
            return true;
        }
        reg_ax = (uint16_t)CurMode->mode;
        CALLBACK_RunRealInt(0x10);
        reg_ax=oldax;
    }
    reg_ax=0x1a00;
    CALLBACK_RunRealInt(0x2F);
    if (reg_al!=0xff) {reg_ax=oldax;return false;}
    reg_ax=oldax;
    return true;
}

std::string GetPlatform(bool save);
char *str_replace(char *orig, char *rep, char *with);
const char *ParseMsg(const char *msg) {
    char str[13];
    strncpy(str, UPDATED_STR, 12);
    str[12]=0;
    if (machine != MCH_PC98) {
        if (!ansiinstalled || J3_IsJapanese()) {
            msg = str_replace(str_replace((char *)msg, (char*)"\033[0m", (char*)""), (char*)"\033[1m", (char*)"");
            for (int i=1; i<8; i++) {
                sprintf(str, "\033[3%dm", i);
                msg = str_replace((char *)msg, str, (char*)"");
                sprintf(str, "\033[4%d;1m", i);
                msg = str_replace((char *)msg, str, (char*)"");
            }
        }
        Section_prop *section = static_cast<Section_prop *>(control->GetSection("dosbox"));
        std::string theme = section->Get_string("bannercolortheme");
        if (theme == "black")
            msg = str_replace((char *)msg, (char*)"\033[44;1m", (char*)"\033[40;1m");
        else if (theme == "red")
            msg = str_replace(str_replace((char *)msg, (char*)"\033[31m", (char*)"\033[34m"), (char*)"\033[44;1m", (char*)"\033[41;1m");
        else if (theme == "green")
            msg = str_replace(str_replace(str_replace((char *)msg, (char*)"\033[36m", (char*)"\033[34m"), (char*)"\033[32m", (char*)"\033[36m"), (char*)"\033[44;1m", (char*)"\033[42;1m");
        else if (theme == "yellow")
            msg = str_replace(str_replace((char *)msg, (char*)"\033[31m", (char*)"\033[34m"), (char*)"\033[44;1m", (char*)"\033[43;1m");
        else if (theme == "blue")
            msg = str_replace((char *)msg, (char*)"\033[44;1m", (char*)"\033[44;1m");
        else if (theme == "magenta")
            msg = str_replace(str_replace((char *)msg, (char*)"\033[31m", (char*)"\033[34m"), (char*)"\033[44;1m", (char*)"\033[45;1m");
        else if (theme == "cyan")
            msg = str_replace(str_replace((char *)msg, (char*)"\033[36m", (char*)"\033[34m"), (char*)"\033[44;1m", (char*)"\033[46;1m");
        else if (theme == "white")
            msg = str_replace(str_replace((char *)msg, (char*)"\033[36m", (char*)"\033[34m"), (char*)"\033[44;1m", (char*)"\033[47;1m");
    }
    if (machine == MCH_PC98)
        return msg;
    else {
        if (real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)>80)
            msg = str_replace(str_replace(str_replace((char *)msg, (char*)"\xBA\033[0m", (char*)"\xBA\033[0m\n"), (char*)"\xBB\033[0m", (char*)"\xBB\033[0m\n"), (char*)"\xBC\033[0m", (char*)"\xBC\033[0m\n");
        bool uselowbox = false;
        force_conversion = true;
        int cp=dos.loaded_codepage;
        if ((showdbcs
#if defined(USE_TTF)
        || ttf.inUse
#endif
        )
#if defined(USE_TTF)
        && halfwidthkana
#endif
        && InitCodePage() && dos.loaded_codepage==932) uselowbox = true;
        force_conversion = false;
        dos.loaded_codepage=cp;
        if (uselowbox || IS_JEGA_ARCH || IS_JDOSV) {
            std::string m=msg;
            if (strstr(msg, "\xCD\xCD\xCD\xCD") != NULL) {
                if (IS_JEGA_ARCH) {
                    msg = str_replace((char *)msg, (char*)"\xC9", (char *)std::string(1, 0x15).c_str());
                    msg = str_replace((char *)msg, (char*)"\xBB", (char *)std::string(1, 0x16).c_str());
                    msg = str_replace((char *)msg, (char*)"\xC8", (char *)std::string(1, 0x18).c_str());
                    msg = str_replace((char *)msg, (char*)"\xBC", (char *)std::string(1, 0x17).c_str());
                    msg = str_replace((char *)msg, (char*)"\xCD", (char *)std::string(1, 0x13).c_str());
                } else if(J3_IsJapanese()) {
                    msg = str_replace((char *)msg, (char*)"\xC9", "+");
                    msg = str_replace((char *)msg, (char*)"\xBB", "+");
                    msg = str_replace((char *)msg, (char*)"\xC8", "+");
                    msg = str_replace((char *)msg, (char*)"\xBC", "+");
                    msg = str_replace((char *)msg, (char*)"\xCD", "-");
                } else {
                    msg = str_replace((char *)msg, (char*)"\xC9", (char *)std::string(1, 1).c_str());
                    msg = str_replace((char *)msg, (char*)"\xBB", (char *)std::string(1, 2).c_str());
                    msg = str_replace((char *)msg, (char*)"\xC8", (char *)std::string(1, 3).c_str());
                    msg = str_replace((char *)msg, (char*)"\xBC", (char *)std::string(1, 4).c_str());
                    msg = str_replace((char *)msg, (char*)"\xCD", (char *)std::string(1, 6).c_str());
                }
            } else {
                if (IS_JEGA_ARCH) {
                    msg = str_replace((char *)msg, (char*)"\xBA ", (char *)(std::string(1, 0x14)+" ").c_str());
                    msg = str_replace((char *)msg, (char*)" \xBA", (char *)(" "+std::string(1, 0x14)).c_str());
                } else if(J3_IsJapanese()) {
                    msg = str_replace((char *)msg, (char*)"\xBA ", "| ");
                    msg = str_replace((char *)msg, (char*)" \xBA", " |");
                } else {
                    msg = str_replace((char *)msg, (char*)"\xBA ", (char *)(std::string(1, 5)+" ").c_str());
                    msg = str_replace((char *)msg, (char*)" \xBA", (char *)(" "+std::string(1, 5)).c_str());
                }
            }
        }
        return msg;
    }
}

static char const * const path_string="PATH=Z:\\;Z:\\SYSTEM;Z:\\BIN;Z:\\DOS;Z:\\4DOS;Z:\\DEBUG;Z:\\TEXTUTIL";
static char const * const comspec_string="COMSPEC=Z:\\COMMAND.COM";
static char const * const prompt_string="PROMPT=$P$G";
static char const * const full_name="Z:\\COMMAND.COM";
static char const * const init_line="/INIT AUTOEXEC.BAT";
extern uint8_t ZDRIVE_NUM;

char *str_replace(char *orig, char *rep, char *with);
void GetExpandedPath(std::string &path) {
    std::string udrive = std::string(1,ZDRIVE_NUM+'A'), ldrive = std::string(1,ZDRIVE_NUM+'a');
    char pathstr[100];
    strcpy(pathstr, path_string+5);
    if (path==udrive+":\\"||path==ldrive+":\\")
        path=ZDRIVE_NUM==25?pathstr:str_replace(pathstr, (char*)"Z:\\", (char *)(udrive+":\\").c_str());
    else if (path.size()>3&&(path.substr(0, 4)==udrive+":\\;"||path.substr(0, 4)==ldrive+":\\;")&&path.substr(4).find(udrive+":\\")==std::string::npos&&path.substr(4).find(ldrive+":\\")==std::string::npos)
        path=std::string(ZDRIVE_NUM==25?pathstr:str_replace(pathstr, (char*)"Z:\\", (char *)(udrive+":\\").c_str()))+path.substr(3);
    else if (path.size()>3) {
        size_t pos = path.find(";"+udrive+":\\");
        if (pos == std::string::npos) pos = path.find(";"+ldrive+":\\");
        if (pos != std::string::npos && (!path.substr(pos+4).size() || (path[pos+4]==';'&&path.substr(pos+4).find(udrive+":\\")==std::string::npos&&path.substr(pos+4).find(ldrive+":\\")==std::string::npos)))
            path=path.substr(0, pos+1)+std::string(ZDRIVE_NUM==25?pathstr:str_replace(pathstr, (char*)"Z:\\", (char *)(udrive+":\\").c_str()))+path.substr(pos+4);
    }
}

void showWelcome(Program *shell) {
    /* Start a normal shell and check for a first command init */
    ansiinstalled = is_ANSI_installed(shell);
    std::string verstr = "v"+std::string(VERSION)+", "+GetPlatform(false);
    if (machine == MCH_PC98) {
        shell->WriteOut(ParseMsg("\x86\x52\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x56\n"));
        shell->WriteOut(ParseMsg((std::string("\x86\x46 \033[32m")+(MSG_Get("SHELL_STARTUP_TITLE")+std::string("             ")).substr(0,30)+std::string("  \033[33m%*s\033[37m \x86\x46\n")).c_str()),34,verstr.c_str());
        shell->WriteOut(ParseMsg("\x86\x46                                                                    \x86\x46\n"));
        shell->WriteOut(ParseMsg((std::string("\x86\x46 ")+MSG_Get("SHELL_STARTUP_HEAD1_PC98")+std::string(" \x86\x46\n")).c_str()));
        shell->WriteOut(ParseMsg("\x86\x46                                                                    \x86\x46\n"));
        shell->WriteOut(ParseMsg((std::string("\x86\x46 ")+str_replace((char *)MSG_Get("SHELL_STARTUP_TEXT1_PC98"), (char*)"\n", (char*)" \x86\x46\n\x86\x46 ")+std::string(" \x86\x46\n")).c_str()));
        shell->WriteOut(ParseMsg((std::string("\x86\x46 ")+MSG_Get("SHELL_STARTUP_EXAMPLE_PC98")+std::string(" \x86\x46\n")).c_str()));
        shell->WriteOut(ParseMsg("\x86\x46                                                                    \x86\x46\n"));
        shell->WriteOut(ParseMsg((std::string("\x86\x46 ")+str_replace((char *)MSG_Get("SHELL_STARTUP_TEXT2_PC98"), (char*)"\n", (char*)" \x86\x46\n\x86\x46 ")+std::string(" \x86\x46\n")).c_str()));
        shell->WriteOut(ParseMsg("\x86\x46                                                                    \x86\x46\n"));
        shell->WriteOut(ParseMsg((std::string("\x86\x46 ")+str_replace((char *)MSG_Get("SHELL_STARTUP_INFO_PC98"), (char*)"\n", (char*)" \x86\x46\n\x86\x46 ")+std::string(" \x86\x46\n")).c_str()));
        shell->WriteOut(ParseMsg("\x86\x46                                                                    \x86\x46\n"));
        shell->WriteOut(ParseMsg((std::string("\x86\x46 ")+str_replace((char *)MSG_Get("SHELL_STARTUP_TEXT3_PC98"), (char*)"\n", (char*)" \x86\x46\n\x86\x46 ")+std::string(" \x86\x46\n")).c_str()));
        shell->WriteOut(ParseMsg("\x86\x5A\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x5E\033[0m\n"));
        shell->WriteOut(ParseMsg((std::string("\033[1m\033[32m")+MSG_Get("SHELL_STARTUP_LAST")+"\033[0m\n").c_str()));
    } else {
        shell->WriteOut(ParseMsg("\033[44;1m\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
            "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
            "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\033[0m"));
        shell->WriteOut(ParseMsg((std::string("\033[44;1m\xBA \033[32m")+(MSG_Get("SHELL_STARTUP_TITLE")+std::string("             ")).substr(0,30)+std::string(" \033[33m%*s\033[37m \xBA\033[0m")).c_str()),45,verstr.c_str());
        shell->WriteOut(ParseMsg("\033[44;1m\xBA                                                                              \xBA\033[0m"));
        shell->WriteOut(ParseMsg((std::string("\033[44;1m\xBA ")+MSG_Get("SHELL_STARTUP_HEAD1")+std::string(" \xBA\033[0m")).c_str()));
        shell->WriteOut(ParseMsg("\033[44;1m\xBA                                                                              \xBA\033[0m"));
        shell->WriteOut(ParseMsg((std::string("\033[44;1m\xBA ")+str_replace((char *)MSG_Get("SHELL_STARTUP_TEXT1"), (char*)"\n", (char*)" \xBA\033[0m\033[44;1m\xBA ")+std::string(" \xBA\033[0m")).c_str()));
        if (IS_VGA_ARCH) shell->WriteOut(ParseMsg((std::string("\033[44;1m\xBA ")+MSG_Get("SHELL_STARTUP_EXAMPLE")+std::string(" \xBA\033[0m")).c_str()));
        shell->WriteOut(ParseMsg("\033[44;1m\xBA                                                                              \xBA\033[0m"));
        shell->WriteOut(ParseMsg((std::string("\033[44;1m\xBA ")+MSG_Get("SHELL_STARTUP_HEAD2")+std::string(" \xBA\033[0m")).c_str()));
        shell->WriteOut(ParseMsg("\033[44;1m\xBA                                                                              \xBA\033[0m"));
        shell->WriteOut(ParseMsg((std::string("\033[44;1m\xBA ")+str_replace((char *)MSG_Get("SHELL_STARTUP_TEXT2"), (char*)"\n", (char*)" \xBA\033[0m\033[44;1m\xBA ")+std::string(" \xBA\033[0m")).c_str()));
        shell->WriteOut(ParseMsg("\033[44;1m\xBA                                                                              \xBA\033[0m"));
        if (IS_DOSV) {
            shell->WriteOut(ParseMsg((std::string("\033[44;1m\xBA ")+str_replace((char *)MSG_Get("SHELL_STARTUP_DOSV"), (char*)"\n", (char*)" \xBA\033[0m\033[44;1m\xBA ")+std::string(" \xBA\033[0m")).c_str()));
            shell->WriteOut(ParseMsg("\033[44;1m\xBA                                                                              \xBA\033[0m"));
        } else if (machine == MCH_CGA || machine == MCH_PCJR || machine == MCH_AMSTRAD) {
            shell->WriteOut(ParseMsg((std::string("\033[44;1m\xBA ")+str_replace((char *)MSG_Get(mono_cga?"SHELL_STARTUP_CGA_MONO":"SHELL_STARTUP_CGA"), (char*)"\n", (char*)" \xBA\033[0m\033[44;1m\xBA ")+std::string(" \xBA\033[0m")).c_str()));
            shell->WriteOut(ParseMsg("\033[44;1m\xBA                                                                              \xBA\033[0m"));
        } else if (machine == MCH_HERC || machine == MCH_MDA) {
            shell->WriteOut(ParseMsg((std::string("\033[44;1m\xBA ")+str_replace((char *)MSG_Get("SHELL_STARTUP_HERC"), (char*)"\n", (char*)" \xBA\033[0m\033[44;1m\xBA ")+std::string(" \xBA\033[0m")).c_str()));
            shell->WriteOut(ParseMsg("\033[44;1m\xBA                                                                              \xBA\033[0m"));
        }
        shell->WriteOut(ParseMsg((std::string("\033[44;1m\xBA ")+MSG_Get("SHELL_STARTUP_HEAD3")+std::string(" \xBA\033[0m")).c_str()));
        shell->WriteOut(ParseMsg("\033[44;1m\xBA                                                                              \xBA\033[0m"));
        shell->WriteOut(ParseMsg((std::string("\033[44;1m\xBA ")+str_replace((char *)MSG_Get("SHELL_STARTUP_TEXT3"), (char*)"\n", (char*)" \xBA\033[0m\033[44;1m\xBA ")+std::string(" \xBA\033[0m")).c_str()));
        shell->WriteOut(ParseMsg("\033[44;1m\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
            "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
            "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC\033[0m"));
        shell->WriteOut(ParseMsg((std::string("\033[32m")+(MSG_Get("SHELL_STARTUP_LAST")+std::string("                                                       ")).substr(0,79)+std::string("\033[0m\n")).c_str()));
    }
}

void DOS_Shell::Prepare(void) {
    if (this == first_shell) {
        Section_prop *section = static_cast<Section_prop *>(control->GetSection("dosbox"));
        if (section->Get_bool("startbanner")&&!control->opt_fastlaunch)
            showWelcome(this);
        else if ((CurMode->type==M_TEXT || IS_PC98_ARCH) && ANSI_SYS_installed())
            WriteOut("\033[2J");
		if (!countryNo) {
#if defined(WIN32)
			char buffer[128];
#endif
            if (IS_PC98_ARCH || IS_JEGA_ARCH)
                countryNo = 81;
            else if (IS_DOSV)
                countryNo = IS_PDOSV?86:(IS_TDOSV?886:(IS_KDOSV?82:81));
#if defined(WIN32)
			else if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_ICOUNTRY, buffer, 128)) {
				countryNo = uint16_t(atoi(buffer));
				DOS_SetCountry(countryNo);
			}
#endif
			else {
                const char *layout = DOS_GetLoadedLayout();
                if (layout == NULL)
                    countryNo = COUNTRYNO::United_States;
                else if (country_code_map.find(layout) != country_code_map.end())
                    countryNo = country_code_map.find(layout)->second;
                else
                    countryNo = COUNTRYNO::United_States;
                DOS_SetCountry(countryNo);
			}
		}
		section = static_cast<Section_prop *>(control->GetSection("dos"));
		bool zdirpath = section->Get_bool("drive z expand path");
		std::string layout = section->Get_string("keyboardlayout");
		strcpy(config_data, "");
		section = static_cast<Section_prop *>(control->GetSection("config"));
		if ((section!=NULL&&!control->opt_noconfig)||control->opt_langcp) {
			char *countrystr = (char *)section->Get_string("country"), *r=strchr(countrystr, ',');
			int country = 0;
			if ((r==NULL || !*(r+1)) && !control->opt_langcp)
				country = atoi(trim(countrystr));
			else {
				if (r!=NULL) *r=0;
				country = atoi(trim(countrystr));
				int32_t newCP = r==NULL||IS_PC98_ARCH||IS_JEGA_ARCH||IS_DOSV?dos.loaded_codepage:atoi(trim(r+1));
                if (control->opt_langcp && msgcodepage>0 && isSupportedCP(msgcodepage) && msgcodepage != newCP)
                    newCP = msgcodepage;
				if (r!=NULL) *r=',';
                if (!IS_PC98_ARCH&&!IS_JEGA_ARCH) {
#if defined(USE_TTF)
                    if (ttf.inUse) {
                        if (newCP) {
                            int missing = toSetCodePage(this, newCP, control->opt_fastlaunch?1:0);
                            WriteOut(MSG_Get("SHELL_CMD_CHCP_ACTIVE"), dos.loaded_codepage);
                            if (missing > 0) WriteOut(MSG_Get("SHELL_CMD_CHCP_MISSING"), missing);
                        }
                        else if (r!=NULL) WriteOut(MSG_Get("SHELL_CMD_CHCP_INVALID"), trim(r+1));
                    } else
#endif
                    if (!newCP && IS_DOSV) {
                        if (IS_JDOSV) newCP=932;
                        else if (IS_PDOSV) newCP=936;
                        else if (IS_KDOSV) newCP=949;
                        else if (IS_TDOSV) newCP=950;
                    }
                    const char* name = DOS_GetLoadedLayout();
                    if (newCP==932||newCP==936||newCP==949||newCP==950||newCP==951) {
                        dos.loaded_codepage=newCP;
                        SetupDBCSTable();
                        runRescan("-A -Q");
                        DOSBox_SetSysMenu();
                    } else if (control->opt_langcp && !name && (layout.empty() || layout=="auto"))
                        SetKEYBCP();
                }
                if (lastmsgcp && lastmsgcp != dos.loaded_codepage) SwitchLanguage(lastmsgcp, dos.loaded_codepage, true);
            }
			if (country>0&&!control->opt_noconfig) {
				countryNo = country;
				DOS_SetCountry(countryNo);
			}
			const char * extra = section->data.c_str();
			if (extra&&!control->opt_securemode&&!control->SecureMode()&&!control->opt_noconfig) {
				std::string vstr;
				std::istringstream in(extra);
				char linestr[CROSS_LEN+1], cmdstr[CROSS_LEN], valstr[CROSS_LEN], tmpstr[CROSS_LEN];
				char *cmd=cmdstr, *val=valstr, *tmp=tmpstr, *p;
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
						strcpy(val, p+1);
						cmd=trim(cmd);
						val=trim(val);
						if (strlen(config_data)+strlen(cmd)+strlen(val)+3<CONFIG_SIZE) {
							strcat(config_data, cmd);
							strcat(config_data, "=");
							strcat(config_data, val);
							strcat(config_data, "\r\n");
						}
						if (!strncasecmp(cmd, "set ", 4)) {
							vstr=std::string(val);
							ResolvePath(vstr);
							if (zdirpath && !strcmp(cmd, "set path")) GetExpandedPath(vstr);
							DoCommand((char *)(std::string(cmd)+"="+vstr).c_str());
						} else if (!strcasecmp(cmd, "install")||!strcasecmp(cmd, "installhigh")||!strcasecmp(cmd, "device")||!strcasecmp(cmd, "devicehigh")) {
							vstr=std::string(val);
							ResolvePath(vstr);
							strcpy(tmp, vstr.c_str());
							char *name=StripArg(tmp);
							if (!*name) continue;
							if (!DOS_FileExists(name)&&!DOS_FileExists((std::string("Z:\\SYSTEM\\")+name).c_str())&&!DOS_FileExists((std::string("Z:\\BIN\\")+name).c_str())&&!DOS_FileExists((std::string("Z:\\DOS\\")+name).c_str())&&!DOS_FileExists((std::string("Z:\\4DOS\\")+name).c_str())&&!DOS_FileExists((std::string("Z:\\DEBUG\\")+name).c_str())&&!DOS_FileExists((std::string("Z:\\TEXTUTIL\\")+name).c_str())) {
								WriteOut(MSG_Get("SHELL_MISSING_FILE"), name);
								continue;
							}
							if (!strcasecmp(cmd, "install"))
								DoCommand((char *)vstr.c_str());
							else if (!strcasecmp(cmd, "installhigh"))
								DoCommand((char *)("lh "+vstr).c_str());
							else if (!strcasecmp(cmd, "device"))
								DoCommand((char *)("device "+vstr).c_str());
							else if (!strcasecmp(cmd, "devicehigh"))
								DoCommand((char *)("lh device "+vstr).c_str());
						}
					} else if (!strncasecmp(line.c_str(), "rem ", 4)) {
						strcat(config_data, line.c_str());
						strcat(config_data, "\r\n");
					}
				}
			}
		}
        std::string line;
        GetEnvStr("PATH",line);
		if (!strlen(config_data)) {
			strcat(config_data, "rem=");
			strcat(config_data, section->Get_string("rem"));
			strcat(config_data, "\r\n");
		}
        internal_program = true;
		VFILE_Register("AUTOEXEC.BAT",(uint8_t *)autoexec_data,(uint32_t)strlen(autoexec_data));
		VFILE_Register("CONFIG.SYS",(uint8_t *)config_data,(uint32_t)strlen(config_data));
        internal_program = false;
#if defined(WIN32)
		if (!control->opt_securemode&&!control->SecureMode())
		{
			const Section_prop* sec = static_cast<Section_prop*>(control->GetSection("dos"));
			const char *automountstr = sec->Get_string("automountall");
			if (strcmp(automountstr, "0") && strcmp(automountstr, "false")) MountAllDrives(!strcmp(automountstr, "quiet")||control->opt_fastlaunch);
		}
#endif
		strcpy(i4dos_data, "");
		section = static_cast<Section_prop *>(control->GetSection("4dos"));
		if (section!=NULL) {
			const char * extra = section->data.c_str();
			if (extra) {
				std::istringstream in(extra);
				if (in)	for (std::string line; std::getline(in, line); ) {
					if (strncasecmp(line.c_str(), "rem=", 4)&&strncasecmp(line.c_str(), "rem ", 4)) {
						strcat(i4dos_data, line.c_str());
						strcat(i4dos_data, "\r\n");
					}
				}
			}
		}
        internal_program = true;
		VFILE_Register("4DOS.INI",(uint8_t *)i4dos_data,(uint32_t)strlen(i4dos_data), "/4DOS/");
        internal_program = false;
        unsigned int cp=dos.loaded_codepage;
        if (!dos.loaded_codepage) InitCodePage();
        initcodepagefont();
        dos.loaded_codepage=cp;
    }
#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11 || defined(MACOSX)) && (defined(C_SDL2) || defined(SDL_DOSBOX_X_SPECIAL))
    if (enableime) SetIMPosition();
#endif
}

void DOS_Shell::Run(void) {
	shellrun=true;
	char input_line[CMD_MAXLINE] = {0};
	std::string line;
	bool optP=cmd->FindStringRemain("/P",line), optC=cmd->FindStringRemainBegin("/C",line), optK=false;
	if (!optC) optK=cmd->FindStringRemainBegin("/K",line);
	if (optP) perm=true;
	if (optC||optK) {
		input_line[CMD_MAXLINE-1u] = 0;
		strncpy(input_line,line.c_str(),CMD_MAXLINE-1u);
		char* sep = strpbrk(input_line,"\r\n"); //GTA installer
		if (sep) *sep = 0;
		DOS_Shell temp;
		temp.echo = echo;
		temp.exec=true;
		temp.ParseLine(input_line);		//for *.exe *.com  |*.bat creates the bf needed by runinternal;
		temp.RunInternal();				// exits when no bf is found.
		temp.exec=false;
		if (!optK||(!perm&&temp.exit)) {
			shellrun=false;
			return;
		}
	} else if (cmd->FindStringRemain("/?",line)) {
		WriteOut(MSG_Get("SHELL_CMD_COMMAND_HELP"));
		shellrun=false;
		return;
	}

	bool optInit=cmd->FindString("/INIT",line,true);
	if (this != first_shell && !optInit)
		WriteOut(optK?"\n":"DOSBox-X command shell [Version %s %s]\nCopyright DOSBox-X Team. All rights reserved.\n\n",VERSION,SDL_STRING);

	if(optInit) {
		input_line[CMD_MAXLINE - 1u] = 0;
		strncpy(input_line, line.c_str(), CMD_MAXLINE - 1u);
		line.erase();
		ParseLine(input_line);
	}
	if (!exit) {
		RunningProgram = "COMMAND";
		GFX_SetTitle(-1,-1,-1,false);
	}
	do {
		/* Get command once a line */
		if (bf) {
			if (bf->ReadLine(input_line)) {
				if (echo) {
					if (input_line[0]!='@') {
						ShowPrompt();
						WriteOut_NoParsing(input_line);
						WriteOut_NoParsing("\n");
					}
				}
			} else input_line[0]='\0';
		} else {
			if (optInit && control->opt_exit) break;
			if (echo) ShowPrompt();
			InputCommand(input_line);
			if (echo && !input_eof) WriteOut("\n");

			/* Bugfix: CTTY NUL will return immediately, the shell input will return
			 *         immediately, and if we don't consume CPU cycles to compensate,
			 *         will leave DOSBox-X running in an endless loop, hung. */
			if (input_eof) CALLBACK_Idle();
		}

		/* do it */
		if(strlen(input_line)!=0) {
			if (bf == NULL/*not running a batch file*/ && shell_keyboard_flush) DOS_FlushSTDIN();
			ParseLine(input_line);
			if (echo && !bf) WriteOut_NoParsing("\n");
			if (bf == NULL/*not running a batch file*/ && shell_keyboard_flush) DOS_FlushSTDIN();
		}
	} while (perm||!exit);
	shellrun=false;
}

void DOS_Shell::SyntaxError(void) {
	WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
}

bool filename_not_8x3(const char *n), isDBCSCP(), isKanji1(uint8_t chr), shiftjis_lead_byte(int c);
class AUTOEXEC:public Module_base {
private:
	AutoexecObject autoexec[17];
	AutoexecObject autoexec_echo;
    AutoexecObject autoexec_auto_bat;
public:
    void RunAdditional() {
        force_conversion = true;
        int cp=dos.loaded_codepage;
        InitCodePage();
        force_conversion = false;
        int ind=0;

        /* The user may have given .BAT files to run on the command line */
        if (!control->auto_bat_additional.empty()) {
            std::string cmd = "@echo off\n";

            for (unsigned int i=0;i<control->auto_bat_additional.size();i++) {
                if (!control->opt_prerun) cmd += "\n";
                if (!strncmp(control->auto_bat_additional[i].c_str(), "@mount c: ", 10)) {
                    cmd += control->auto_bat_additional[i]+"\n";
                    cmd += "@config -get lastmount>nul\n";
                    cmd += "@if not '%CONFIG%'=='' %CONFIG%";
                } else {
                    std::string batname;
                    //LOG_MSG("auto_bat_additional %s\n", control->auto_bat_additional[i].c_str());

                    std::replace(control->auto_bat_additional[i].begin(),control->auto_bat_additional[i].end(),'/','\\');
                    size_t pos = std::string::npos;
                    bool lead = false;
                    for (unsigned int j=0; j<control->auto_bat_additional[i].size(); j++) {
                        if (lead) lead = false;
                        else if ((IS_PC98_ARCH && shiftjis_lead_byte(control->auto_bat_additional[i][j])) || (isDBCSCP() && isKanji1(control->auto_bat_additional[i][j]))) lead = true;
                        else if (control->auto_bat_additional[i][j]=='\\') pos = j;
                    }
                    if(pos == std::string::npos) {  //Only a filename, mount current directory
                        batname = control->auto_bat_additional[i];
                        cmd += "@mount c: . -nl -q\n";
                    } else { //Parse the path of .BAT file
                        std::string batpath = control->auto_bat_additional[i].substr(0,pos+1);
                        if (batpath==".\\") batpath=".";
                        else if (batpath=="..\\") batpath="..";
                        batname = control->auto_bat_additional[i].substr(pos+1);
                        cmd += "@mount c: \"" + batpath + "\" -nl -q\n";
                    }
                    std::string opt = control->opt_o.size() > ind && control->opt_o[ind].size() ? " "+control->opt_o[ind] : "";
                    ind++;
                    bool templfn=!uselfn&&filename_not_8x3(batname.c_str())&&(enablelfn==-1||enablelfn==-2);
                    cmd += "@config -get lastmount>nul\n";
                    cmd += "@set LASTMOUNT=%CONFIG%\n";
                    cmd += "@if not '%LASTMOUNT%'=='' %LASTMOUNT%\n";
                    cmd += "@cd \\\n";
                    if (templfn) cmd += "@config -set lfn=true\n";
#if defined(WIN32) && !defined(HX_DOS)
                    if (!winautorun) cmd += "@config -set startcmd=true\n";
#endif
                    cmd += "@CALL \"";
                    cmd += batname;
                    cmd += "\"" + opt + "\n";
                    if (templfn) cmd += "@config -set lfn=" + std::string(enablelfn==-1?"auto":"autostart") + "\n";
#if defined(WIN32) && !defined(HX_DOS)
                    if (!winautorun) cmd += "@config -set startcmd=false\n";
#endif
                    cmd += "@if not '%LASTMOUNT%'=='' mount %LASTMOUNT% -q -u\n";
                    cmd += "@set LASTMOUNT=";
                }
                if (control->opt_prerun) cmd += "\n";
            }

            autoexec_auto_bat.Install(cmd);
        }
        dos.loaded_codepage = cp;
    }
	AUTOEXEC(Section* configuration):Module_base(configuration) {
		/* Register a virtual AUTOEXEC.BAT file */
		const Section_line * section=static_cast<Section_line *>(configuration);

		/* Check -securemode switch to disable mount/imgmount/boot after running autoexec.bat */
		bool secure = control->opt_securemode;

		if (control->opt_prerun) RunAdditional();

		/* add stuff from the configfile unless -noautexec or -securemode is specified. */
		const char * extra = section->data.c_str();
		if (extra && !secure && !control->opt_noautoexec) {
			/* detect if "echo off" is the first line */
			size_t firstline_length = strcspn(extra,"\r\n");
			bool echo_off  = !strncasecmp(extra,"echo off",8);
			if (echo_off && firstline_length == 8) extra += 8;
			else {
				echo_off = !strncasecmp(extra,"@echo off",9);
				if (echo_off && firstline_length == 9) extra += 9;
				else echo_off = false;
			}

			/* if "echo off" move it to the front of autoexec.bat */
			if (echo_off)  { 
				autoexec_echo.InstallBefore("@echo off");
				if (*extra == '\r') extra++; //It can point to \0
				if (*extra == '\n') extra++; //same
			}

			/* Install the stuff from the configfile if anything left after moving echo off */

			if (*extra) autoexec[0].Install(std::string(extra));
		}

		/* Check to see for extra command line options to be added (before the command specified on commandline) */
		/* Maximum of extra commands: 10 */
		Bitu i = 1;
		for (auto it=control->opt_c.begin();i <= 11 && it!=control->opt_c.end();it++) /* -c switches */
			autoexec[i++].Install(*it);

		/* Check for the -exit switch which causes dosbox to when the command on the commandline has finished */
		bool addexit = control->opt_exit;

		if (!control->opt_prerun) RunAdditional();

#if 0/*FIXME: This is ugly. I don't care to follow through on this nonsense for now. When needed, port to new command line switching. */
		/* Check for first command being a directory or file */
		char buffer[CROSS_LEN+1];
		char orig[CROSS_LEN+1];
		char cross_filesplit[2] = {CROSS_FILESPLIT , 0};

		Bitu dummy = 1;
		bool command_found = false;
		while (control->cmdline->FindCommand(dummy++,line) && !command_found) {
			struct stat test;
			if (line.length() > CROSS_LEN) continue; 
			strcpy(buffer,line.c_str());
			if (stat(buffer,&test)) {
				if (getcwd(buffer,CROSS_LEN) == NULL) continue;
				if (strlen(buffer) + line.length() + 1 > CROSS_LEN) continue;
				strcat(buffer,cross_filesplit);
				strcat(buffer,line.c_str());
				if (stat(buffer,&test)) continue;
			}
			if (test.st_mode & S_IFDIR) {
				autoexec[12].Install(std::string("MOUNT C \"") + buffer + "\"");
				autoexec[13].Install("C:");
				if(secure) autoexec[14].Install("z:\\system\\config.com -securemode");
				command_found = true;
			} else {
				char* name = strrchr(buffer,CROSS_FILESPLIT);
				if (!name) { //Only a filename 
					line = buffer;
					if (getcwd(buffer,CROSS_LEN) == NULL) continue;
					if (strlen(buffer) + line.length() + 1 > CROSS_LEN) continue;
					strcat(buffer,cross_filesplit);
					strcat(buffer,line.c_str());
					if(stat(buffer,&test)) continue;
					name = strrchr(buffer,CROSS_FILESPLIT);
					if(!name) continue;
				}
				*name++ = 0;
				if (access(buffer,F_OK)) continue;
				autoexec[12].Install(std::string("MOUNT C \"") + buffer + "\"");
				autoexec[13].Install("C:");
				/* Save the non-modified filename (so boot and imgmount can use it (long filenames, case sensivitive)) */
				strcpy(orig,name);
				upcase(name);
				if(strstr(name,".BAT") != 0) {
					if(secure) autoexec[14].Install("z:\\system\\config.com -securemode");
					/* BATch files are called else exit will not work */
					autoexec[15].Install(std::string("CALL ") + name);
					if(addexit) autoexec[16].Install("exit");
				} else if((strstr(name,".IMG") != 0) || (strstr(name,".IMA") !=0 )) {
					//No secure mode here as boot is destructive and enabling securemode disables boot
					/* Boot image files */
					autoexec[15].Install(std::string("BOOT ") + orig);
				} else if((strstr(name,".ISO") != 0) || (strstr(name,".CUE") !=0 )) {
					/* imgmount CD image files */
					/* securemode gets a different number from the previous branches! */
					autoexec[14].Install(std::string("IMGMOUNT D \"") + orig + std::string("\" -t iso"));
					//autoexec[16].Install("D:");
					if(secure) autoexec[15].Install("z:\\system\\config.com -securemode");
					/* Makes no sense to exit here */
				} else {
					if(secure) autoexec[14].Install("z:\\system\\config.com -securemode");
					autoexec[15].Install(name);
					if(addexit) autoexec[16].Install("exit");
				}
				command_found = true;
			}
		}

		/* Combining -securemode, noautoexec and no parameters leaves you with a lovely Z:\. */
		if ( !command_found ) { 
			if ( secure ) autoexec[12].Install("z:\\system\\config.com -securemode");
		}
#else
		if (secure) autoexec[i++].Install("z:\\system\\config.com -securemode");
#endif

		if (addexit) autoexec[i++].Install("exit");

		assert(i <= 17); /* FIXME: autoexec[] should not be fixed size */

        internal_program = true;
		VFILE_Register("AUTOEXEC.BAT",(uint8_t *)autoexec_data,(uint32_t)strlen(autoexec_data));
        internal_program = true;

	}
};

static AUTOEXEC* test = NULL;
	
static void AUTOEXEC_ShutDown(Section * sec) {
    (void)sec;//UNUSED
	if (test != NULL) {
		delete test;
		test = NULL;
	}
    if (first_shell != NULL) {
		delete first_shell;
		first_shell = 0;//Make clear that it shouldn't be used anymore
    }
    if (call_shellstop != 0) {
        CALLBACK_DeAllocate(call_shellstop);
        call_shellstop = 0;
    }
}

void AUTOEXEC_Startup(Section *sec) {
    (void)sec;//UNUSED
	if (test == NULL) {
		LOG(LOG_MISC,LOG_DEBUG)("Allocating AUTOEXEC.BAT emulation");
		test = new AUTOEXEC(control->GetSection("autoexec"));
	}
}

void AUTOEXEC_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing AUTOEXEC.BAT emulation");

	AddExitFunction(AddExitFunctionFuncPair(AUTOEXEC_ShutDown));
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(AUTOEXEC_ShutDown));
	AddVMEventFunction(VM_EVENT_DOS_EXIT_BEGIN,AddVMEventFunctionFuncPair(AUTOEXEC_ShutDown));
	AddVMEventFunction(VM_EVENT_DOS_EXIT_REBOOT_BEGIN,AddVMEventFunctionFuncPair(AUTOEXEC_ShutDown));
	AddVMEventFunction(VM_EVENT_DOS_SURPRISE_REBOOT,AddVMEventFunctionFuncPair(AUTOEXEC_ShutDown));
}

static Bitu INT2E_Handler(void) {
	/* Save return address and current process */
	RealPt save_ret=real_readd(SegValue(ss),reg_sp);
	uint16_t save_psp=dos.psp();

	/* Set first shell as process and copy command */
	dos.psp(shell_psp);//DOS_FIRST_SHELL);
	DOS_PSP psp(shell_psp);//DOS_FIRST_SHELL);
	psp.SetCommandTail(RealMakeSeg(ds,reg_si));
	SegSet16(ss,RealSeg(psp.GetStack()));
	reg_sp=2046;

	/* Read and fix up command string */
	CommandTail tail;
	MEM_BlockRead(PhysMake(dos.psp(),CTBUF+1),&tail,CTBUF+1);
	if (tail.count<CTBUF) tail.buffer[tail.count]=0;
	else tail.buffer[CTBUF-1]=0;
	char* crlf=strpbrk(tail.buffer,"\r\n");
	if (crlf) *crlf=0;

	/* Execute command */
	if (strlen(tail.buffer)) {
		DOS_Shell temp;
		temp.ParseLine(tail.buffer);
		temp.RunInternal();
	}

	/* Restore process and "return" to caller */
	dos.psp(save_psp);
	SegSet16(cs,RealSeg(save_ret));
	reg_ip=RealOff(save_ret);
	reg_ax=0;
	return CBRET_NONE;
}

/* TODO: Why is all this DOS kernel and VFILE registration here in SHELL_Init()?
 *       That's like claiming that DOS memory and device initialization happens from COMMAND.COM!
 *       We need to move the DOS kernel initialization into another function, and the VFILE
 *       registration to another function, and then message initialization to another function,
 *       and then those functions need to be called before SHELL_Init() -J.C. */
void SHELL_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing DOS shell");

	/* Add messages */
	MSG_Add("SHELL_CMD_TREE_ERROR", "No subdirectories exist\n");
	MSG_Add("SHELL_CMD_VOL_TREE", "Directory PATH listing for Volume %s\n");
	MSG_Add("SHELL_CMD_VOL_DRIVE","\n Volume in drive %c ");
	MSG_Add("SHELL_CMD_VOL_SERIAL"," Volume Serial Number is ");
	MSG_Add("SHELL_CMD_VOL_SERIAL_NOLABEL","has no label\n");
	MSG_Add("SHELL_CMD_VOL_SERIAL_LABEL","is %s\n");
	MSG_Add("SHELL_ILLEGAL_PATH","Path not found\n");
	MSG_Add("SHELL_ILLEGAL_DRIVE","Invalid drive specification\n");
	MSG_Add("SHELL_CMD_HELP","If you want a list of all supported internal commands type \033[33;1mHELP /ALL\033[0m.\nYou can also find external commands on the Z: drive as programs.\nA short list of the most often used commands:\n");
	MSG_Add("SHELL_CMD_HELP_END1","External commands such as \033[33;1mMOUNT\033[0m and \033[33;1mIMGMOUNT\033[0m can be found on the Z: drive.\n");
	MSG_Add("SHELL_CMD_HELP_END2","Type \033[33;1mHELP command\033[0m or \033[33;1mcommand /?\033[0m for help information for the specified command.\n");
	MSG_Add("SHELL_CMD_ECHO_ON","ECHO is on.\n");
	MSG_Add("SHELL_CMD_ECHO_OFF","ECHO is off.\n");
	MSG_Add("SHELL_ILLEGAL_SWITCH","Invalid switch - %s\n");
	MSG_Add("SHELL_INVALID_PARAMETER","Invalid parameter - %s\n");
	MSG_Add("SHELL_MISSING_PARAMETER","Required parameter missing.\n");
	MSG_Add("SHELL_MISSING_FILE","The following file is missing or corrupted: %s\n");
	MSG_Add("SHELL_CMD_CHDIR_ERROR","Invalid directory - %s\n");
	MSG_Add("SHELL_CMD_CHDIR_HINT","Hint: To change to different drive type \033[31m%c:\033[0m\n");
	MSG_Add("SHELL_CMD_CHDIR_HINT_2","directoryname contains unquoted spaces.\nTry \033[31mcd %s\033[0m or properly quote them with quotation marks.\n");
	MSG_Add("SHELL_CMD_CHDIR_HINT_3","You are still on drive Z:, and the specified directory cannot be found.\nFor accessing a mounted drive, change to the drive with a syntax like \033[31mC:\033[0m.\n");
	MSG_Add("SHELL_CMD_DATE_HELP","Displays or changes the internal date.\n");
	MSG_Add("SHELL_CMD_DATE_ERROR","The specified date is not correct.\n");
	MSG_Add("SHELL_CMD_DATE_DAYS","3SunMonTueWedThuFriSat"); // "2SoMoDiMiDoFrSa"
	MSG_Add("SHELL_CMD_DATE_NOW","Current date: ");
	MSG_Add("SHELL_CMD_DATE_SETHLP","Type 'date %s' to change.\n");
	MSG_Add("SHELL_CMD_DATE_HELP_LONG","DATE [[/T] [/H] [/S] | date]\n"\
									"  date:       New date to set\n"\
									"  /S:         Permanently use host time and date as DOS time\n"\
                                    "  /F:         Switch back to DOSBox-X internal time (opposite of /S)\n"\
									"  /T:         Only display date\n"\
									"  /H:         Synchronize with host\n");
	MSG_Add("SHELL_CMD_TIME_HELP","Displays or changes the internal time.\n");
	MSG_Add("SHELL_CMD_TIME_ERROR","The specified time is not correct.\n");
	MSG_Add("SHELL_CMD_TIME_NOW","Current time: ");
	MSG_Add("SHELL_CMD_TIME_SETHLP","Type 'time %s' to change.\n");
	MSG_Add("SHELL_CMD_TIME_HELP_LONG","TIME [[/T] [/H] | time]\n"\
									"  time:       New time to set\n"\
									"  /T:         Display simple time\n"\
									"  /H:         Synchronize with host\n");
	MSG_Add("SHELL_CMD_MKDIR_EXIST","Directory already exists - %s\n");
	MSG_Add("SHELL_CMD_MKDIR_ERROR","Unable to create directory - %s\n");
	MSG_Add("SHELL_CMD_RMDIR_ERROR","Invalid path, not directory, or directory not empty - %s\n");
    MSG_Add("SHELL_CMD_RENAME_ERROR","Unable to rename - %s\n");
	MSG_Add("SHELL_CMD_ATTRIB_GET_ERROR","Unable to get attributes: %s\n");
	MSG_Add("SHELL_CMD_ATTRIB_SET_ERROR","Unable to set attributes: %s\n");
	MSG_Add("SHELL_CMD_DEL_ERROR","Unable to delete - %s\n");
	MSG_Add("SHELL_CMD_DEL_SURE","All files in directory will be deleted!\nAre you sure [Y/N]?");
	MSG_Add("SHELL_SYNTAXERROR","Syntax error\n");
	MSG_Add("SHELL_CMD_SET_NOT_SET","Environment variable %s not defined.\n");
	MSG_Add("SHELL_CMD_SET_OUT_OF_SPACE","Not enough environment space left.\n");
	MSG_Add("SHELL_CMD_IF_EXIST_MISSING_FILENAME","IF EXIST: Missing filename.\n");
	MSG_Add("SHELL_CMD_IF_ERRORLEVEL_MISSING_NUMBER","IF ERRORLEVEL: Missing number.\n");
	MSG_Add("SHELL_CMD_IF_ERRORLEVEL_INVALID_NUMBER","IF ERRORLEVEL: Invalid number.\n");
	MSG_Add("SHELL_CMD_GOTO_MISSING_LABEL","No label supplied to GOTO command.\n");
	MSG_Add("SHELL_CMD_GOTO_LABEL_NOT_FOUND","GOTO: Label %s not found.\n");
	MSG_Add("SHELL_CMD_FILE_ACCESS_DENIED","Access denied - %s\n");
	MSG_Add("SHELL_CMD_FILE_NOT_FOUND","File not found - %s\n");
	MSG_Add("SHELL_CMD_FILE_EXISTS","File %s already exists.\n");
	MSG_Add("SHELL_CMD_DIR_INTRO"," Directory of %s\n\n");
	MSG_Add("SHELL_CMD_DIR_BYTES_USED","%5d File(s) %17s Bytes\n");
	MSG_Add("SHELL_CMD_DIR_BYTES_FREE","%5d Dir(s)  %17s Bytes free\n");
	MSG_Add("SHELL_CMD_DIR_FILES_LISTED","Total files listed:\n");
	MSG_Add("SHELL_EXECUTE_DRIVE_NOT_FOUND","Drive %c does not exist!\nYou must \033[31mmount\033[0m it first. Type \033[1;33mintro\033[0m or \033[1;33mintro mount\033[0m for more information.\n");
	MSG_Add("SHELL_EXECUTE_DRIVE_ACCESS_CDROM","Do you want to give DOSBox-X access to your real CD-ROM drive %c [Y/N]?");
	MSG_Add("SHELL_EXECUTE_DRIVE_ACCESS_FLOPPY","Do you want to give DOSBox-X access to your real floppy drive %c [Y/N]?");
	MSG_Add("SHELL_EXECUTE_DRIVE_ACCESS_REMOVABLE","Do you want to give DOSBox-X access to your real removable drive %c [Y/N]?");
	MSG_Add("SHELL_EXECUTE_DRIVE_ACCESS_NETWORK","Do you want to give DOSBox-X access to your real network drive %c [Y/N]?");
	MSG_Add("SHELL_EXECUTE_DRIVE_ACCESS_FIXED","Do you really want to give DOSBox-X access to your real hard drive %c [Y/N]?");
	MSG_Add("SHELL_EXECUTE_ILLEGAL_COMMAND","Bad command or filename - \"%s\"\n");
	MSG_Add("SHELL_CMD_PAUSE","Press any key to continue . . .\n");
	MSG_Add("SHELL_CMD_PAUSE_HELP","Waits for one keystroke to continue.\n");
	MSG_Add("SHELL_CMD_PAUSE_HELP_LONG","PAUSE\n");
	MSG_Add("SHELL_CMD_COPY_FAILURE","Copy failure - %s\n");
	MSG_Add("SHELL_CMD_COPY_SUCCESS","   %d File(s) copied.\n");
	MSG_Add("SHELL_CMD_COPY_CONFIRM","Overwrite %s (Yes/No/All)?");
	MSG_Add("SHELL_CMD_COPY_NOSPACE","Insufficient disk space - %s\n");
	MSG_Add("SHELL_CMD_COPY_ERROR","Copy error - %s\n");
	MSG_Add("SHELL_CMD_SUBST_DRIVE_LIST","The currently mounted local drives are:\n");
	MSG_Add("SHELL_CMD_SUBST_NO_REMOVE","Unable to remove, drive not in use.\n");
	MSG_Add("SHELL_CMD_SUBST_IN_USE","Target drive is already in use.\n");
	MSG_Add("SHELL_CMD_SUBST_NOT_LOCAL","It is only possible to use SUBST on local drives.\n");
	MSG_Add("SHELL_CMD_SUBST_INVALID_PATH","The specified drive or path is invalid.\n");
	MSG_Add("SHELL_CMD_SUBST_FAILURE","SUBST: There is an error in your command line.\n");
	MSG_Add("SHELL_CMD_VTEXT_ON","DOS/V V-text is currently enabled.\n");
	MSG_Add("SHELL_CMD_VTEXT_OFF","DOS/V V-text is currently disabled.\n");

    std::string mapper_keybind = mapper_event_keybind_string("host");
    if (mapper_keybind.empty()) mapper_keybind = "unbound";

    /* Capitalize the binding */
    if (mapper_keybind.size() > 0)
        mapper_keybind[0] = toupper(mapper_keybind[0]);

    std::string default_host =
#if defined(WIN32) && !defined(HX_DOS)
    "F11"
#else
    "F12"
#endif
    ;

    /* Punctuation is important too. */
    //mapper_keybind += ".";

    /* NTS: MSG_Add() takes the string as const char * but it does make a copy of the string when entering into the message map,
     *      so there is no problem here of causing use-after-free crashes when we exit. */
    std::string host_key_help; // SHELL_STARTUP_BEGIN2

    if (machine == MCH_PC98) {
// "\x86\x46 To activate the keymapper \033[31mhost+M\033[37m. Host key is F12.                 \x86\x46\n"
    }
    else {
// "\xBA To activate the keymapper \033[31mhost+M\033[37m. Host key is F12.                 \xBA\n"
    }

    MSG_Add("SHELL_STARTUP_TITLE", "Welcome to DOSBox-X !");
    MSG_Add("SHELL_STARTUP_HEAD1_PC98", "\033[36mGetting Started with DOSBox-X:\033[37m                                    ");
    MSG_Add("SHELL_STARTUP_TEXT1_PC98", "Type \033[32mHELP\033[37m for shell commands, and \033[32mINTRO\033[37m for a short introduction. \nYou could also complete various tasks through the \033[33mdrop-down menus\033[37m.");
    MSG_Add("SHELL_STARTUP_EXAMPLE_PC98", "\033[32mExample\033[37m: Try select \033[33mTrueType font\033[37m or \033[33mOpenGL perfect\033[37m output option.");
    MSG_Add("SHELL_STARTUP_TEXT2_PC98", (std::string("To launch the \033[33mConfiguration Tool\033[37m, use \033[31mhost+C\033[37m. Host key is \033[32m") + (mapper_keybind + "\033[37m.                       ").substr(0,13) + std::string("\nTo activate the \033[33mMapper Editor\033[37m for key assignments, use \033[31mhost+M\033[37m.    \nTo switch between windowed and full-screen mode, use \033[31mhost+F\033[37m.      \nTo adjust the emulated CPU speed, use \033[31mhost+Plus\033[37m and \033[31mhost+Minus\033[37m.   ")).c_str());
    MSG_Add("SHELL_STARTUP_INFO_PC98","\033[36mDOSBox-X is now running in \033[32mJapanese NEC PC-98\033[36m emulation mode.\033[37m     ");
    MSG_Add("SHELL_STARTUP_TEXT3_PC98", "\033[32mDOSBox-X project \033[33mhttps://dosbox-x.com/     \033[36mComplete DOS emulations\033[37m\n\033[32mDOSBox-X guide   \033[33mhttps://dosbox-x.com/wiki\033[37m \033[36mDOS, Windows 3.x and 9x\033[37m\n\033[32mDOSBox-X support \033[33mhttps://github.com/joncampbell123/dosbox-x/issues\033[37m");
    MSG_Add("SHELL_STARTUP_HEAD1", "\033[36mGetting started with DOSBox-X:                                              \033[37m");
    MSG_Add("SHELL_STARTUP_TEXT1", "Type \033[32mHELP\033[37m to see the list of shell commands, \033[32mINTRO\033[37m for a brief introduction.\nYou can also complete various tasks in DOSBox-X through the \033[33mdrop-down menus\033[37m.");
    MSG_Add("SHELL_STARTUP_EXAMPLE", "\033[32mExample\033[37m: Try select the \033[33mTrueType font\033[37m or \033[33mOpenGL pixel-perfect\033[37m output option.");
    MSG_Add("SHELL_STARTUP_HEAD2", "\033[36mUseful default shortcuts:                                                   \033[37m");
    MSG_Add("SHELL_STARTUP_TEXT2", (std::string("- switch between windowed and full-screen mode with key combination \033[31m")+(default_host+" \033[37m+ \033[31mF\033[37m                        ").substr(0,23)+std::string("\033[37m\n") +
            std::string("- launch \033[33mConfiguration Tool\033[37m using \033[31m")+(default_host+" \033[37m+ \033[31mC\033[37m                      ").substr(0,22)+std::string("\033[37m, and \033[33mMapper Editor\033[37m using \033[31m")+(default_host+" \033[37m+ \033[31mM\033[37m                     ").substr(0,24)+std::string("\033[37m\n") +
            std::string("- increase or decrease the emulation speed with \033[31m")+(default_host+" \033[37m+ \033[31mPlus\033[37m      ").substr(0,25)+std::string("\033[37m or \033[31m") +
            (default_host+" \033[37m+ \033[31mMinus\033[37m       ").substr(0,29)+std::string("\033[37m")).c_str());
    MSG_Add("SHELL_STARTUP_DOSV","\033[32mDOS/V mode\033[37m is now active. Try also \033[32mTTF CJK mode\033[37m for a general DOS emulation.");
    MSG_Add("SHELL_STARTUP_CGA", "Composite CGA mode is supported. Use \033[31mCtrl+F8\033[37m to set composite output ON/OFF.\nUse \033[31mCtrl+Shift+[F7/F8]\033[37m to change hue; \033[31mCtrl+F7\033[37m selects early/late CGA model. ");
    MSG_Add("SHELL_STARTUP_CGA_MONO","Use \033[31mCtrl+F7\033[37m to cycle through green, amber, and white monochrome color,      \nand \033[31mCtrl+F8\033[37m to change contrast/brightness settings.                         ");
    MSG_Add("SHELL_STARTUP_HERC","Use \033[31mCtrl+F7\033[37m to cycle through white, amber, and green monochrome color.      \nUse \033[31mCtrl+F8\033[37m to toggle horizontal blending (only in graphics mode).          ");
    MSG_Add("SHELL_STARTUP_HEAD3", "\033[36mDOSBox-X project on the web:                                                \033[37m");
    MSG_Add("SHELL_STARTUP_TEXT3", "\033[32mHomepage of project\033[37m: \033[33mhttps://dosbox-x.com/           \033[36mComplete DOS emulations\033[37m\n\033[32mUser guides on Wiki\033[37m: \033[33mhttps://dosbox-x.com/wiki\033[32m       \033[36mDOS, Windows 3.x and 9x\033[37m\n\033[32mIssue or suggestion\033[37m: \033[33mhttps://github.com/joncampbell123/dosbox-x/issues      \033[37m");
    MSG_Add("SHELL_STARTUP_LAST", "HAVE FUN WITH DOSBox-X !");

	MSG_Add("SHELL_CMD_BREAK_HELP","Sets or clears extended CTRL+C checking.\n");
	MSG_Add("SHELL_CMD_BREAK_HELP_LONG","BREAK [ON | OFF]\n\nType BREAK without a parameter to display the current BREAK setting.\n");
	MSG_Add("SHELL_CMD_CHDIR_HELP","Displays or changes the current directory.\n");
	MSG_Add("SHELL_CMD_CHDIR_HELP_LONG","CHDIR [drive:][path]\n"
	        "CHDIR [..]\n"
	        "CD [drive:][path]\n"
	        "CD [..]\n\n"
	        "  ..   Specifies that you want to change to the parent directory.\n\n"
	        "Type CD drive: to display the current directory in the specified drive.\n"
	        "Type CD without parameters to display the current drive and directory.\n");
	MSG_Add("SHELL_CMD_CLS_HELP","Clears screen.\n");
	MSG_Add("SHELL_CMD_CLS_HELP_LONG","CLS\n");
	MSG_Add("SHELL_CMD_DIR_HELP","Displays a list of files and subdirectories in a directory.\n");
	MSG_Add("SHELL_CMD_DIR_HELP_LONG","DIR [drive:][path][filename] [/[W|B]] [/S] [/P] [/A[D|H|S|R|A]] [/O[N|E|G|S|D]]\n\n"
		   "  [drive:][path][filename]\n"
		   "              Specifies drive, directory, and/or files to list.\n"
		   "  /W          Uses wide list format.\n"
		   "  /B          Uses bare format (no heading information or summary).\n"
		   "  /S          Displays files in specified directory and all subdirectories.\n"
		   "  /P          Pauses after each screenful of information.\n"
		   "  /A          Displays files with specified attributes.\n"
		   "  attributes   D  Directories                R  Read-only files\n"
		   "               H  Hidden files               A  Files ready for archiving\n"
		   "               S  System files               -  Prefix meaning not\n"
		   "  /O          List by files in sorted order.\n"
		   "  sortorder    N  By name (alphabetic)       S  By size (smallest first)\n"
		   "               E  By extension (alphabetic)  D  By date & time (earliest first)\n"
		   "               G  Group directories first    -  Prefix to reverse order\n\n"
		   "Switches may be preset in the DIRCMD environment variable.  Override\n"
		   "preset switches by prefixing any switch with - (hyphen)--for example, /-W.\n"
		   );
	MSG_Add("SHELL_CMD_ECHO_HELP","Displays messages, or turns command-echoing on or off.\n");
	MSG_Add("SHELL_CMD_ECHO_HELP_LONG","  ECHO [ON | OFF]\n  ECHO [message]\n\nType ECHO without parameters to display the current echo setting.\n");
	MSG_Add("SHELL_CMD_EXIT_HELP","Exits from the command shell.\n");
	MSG_Add("SHELL_CMD_EXIT_HELP_LONG","EXIT\n");
	MSG_Add("SHELL_CMD_HELP_HELP","Shows DOSBox-X command help.\n");
	MSG_Add("SHELL_CMD_HELP_HELP_LONG","HELP [/A or /ALL]\nHELP [command]\n\n"
		    "   /A or /ALL   Lists all supported internal commands.\n"
		    "   [command]    Shows help for the specified command.\n\n"
            "\033[0mE.g., \033[37;1mHELP COPY\033[0m or \033[37;1mCOPY /?\033[0m shows help information for COPY command.\n\n"
			"Note: External commands like \033[33;1mMOUNT\033[0m and \033[33;1mIMGMOUNT\033[0m are not listed by HELP [/A].\n"
			"      These commands can be found on the Z: drive as programs (e.g. MOUNT.COM).\n"
            "      Type \033[33;1mcommand /?\033[0m or \033[33;1mHELP command\033[0m for help information for that command.\n");
    MSG_Add("SHELL_CMD_LS_HELP","Lists directory contents.\n");
    MSG_Add("SHELL_CMD_LS_HELP_LONG","LS [drive:][path][filename] [/A] [/L] [/P] [/Z]\n\n"
            "  /A     Lists hidden and system files also.\n"
            "  /L     Lists names one per line.\n"
            "  /P     Pauses after each screenful of information.\n"
            "  /Z     Displays short names even if LFN support is available.\n");
	MSG_Add("SHELL_CMD_MKDIR_HELP","Creates a directory.\n");
	MSG_Add("SHELL_CMD_MKDIR_HELP_LONG","MKDIR [drive:][path]\n"
	        "MD [drive:][path]\n");
	MSG_Add("SHELL_CMD_RMDIR_HELP","Removes a directory.\n");
	MSG_Add("SHELL_CMD_RMDIR_HELP_LONG","RMDIR [drive:][path]\n"
	        "RD [drive:][path]\n");
	MSG_Add("SHELL_CMD_SET_HELP","Displays or changes environment variables.\n");
	MSG_Add("SHELL_CMD_SET_HELP_LONG","SET [variable=[string]]\n\n"
		   "   variable     Specifies the environment-variable name.\n"
		   "   string       Specifies a series of characters to assign to the variable.\n\n"
		   "* If no string is specified, the variable is removed from the environment.\n\n"
		   "Type SET without parameters to display the current environment variables.\n");
	MSG_Add("SHELL_CMD_IF_HELP","Performs conditional processing in batch programs.\n");
	MSG_Add("SHELL_CMD_IF_HELP_LONG","IF [NOT] ERRORLEVEL number command\n"
		   "IF [NOT] string1==string2 command\n"
		   "IF [NOT] EXIST filename command\n\n"
		   "  NOT               Specifies that DOS should carry out\n"
		   "                    the command only if the condition is false.\n\n"
		   "  ERRORLEVEL number Specifies a true condition if the last program run\n"
		   "                    returned an exit code equal to or greater than the number\n"
		   "                    specified.\n\n"
		   "  string1==string2  Specifies a true condition if the specified text strings\n"
		   "                    match.\n\n"
		   "  EXIST filename    Specifies a true condition if the specified filename\n"
		   "                    exists.\n\n"
		   "  command           Specifies the command to carry out if the condition is\n"
		   "                    met.  Command can be followed by ELSE command which\n"
		   "                    will execute the command after the ELSE keyword if the\n"
		   "                    specified condition is FALSE\n");
	MSG_Add("SHELL_CMD_GOTO_HELP","Jumps to a labeled line in a batch program.\n");
	MSG_Add("SHELL_CMD_GOTO_HELP_LONG","GOTO label\n\n"
		   "   label   Specifies a text string used in the batch program as a label.\n\n"
		   "You type a label on a line by itself, beginning with a colon.\n");
	MSG_Add("SHELL_CMD_HISTORY_HELP","Displays or clears the command history list.\n");
	MSG_Add("SHELL_CMD_HISTORY_HELP_LONG","HISTORY [/C]\n\n  /C  Clears the command history list.\n");
	MSG_Add("SHELL_CMD_SHIFT_HELP","Changes the position of replaceable parameters in a batch file.\n");
	MSG_Add("SHELL_CMD_SHIFT_HELP_LONG","SHIFT\n");
	MSG_Add("SHELL_CMD_FOR_HELP","Runs a specified command for each file in a set of files.\n");
	MSG_Add("SHELL_CMD_FOR_HELP_LONG","FOR %%variable IN (set) DO command [command-parameters]\n\n  %%variable  Specifies a replaceable parameter.\n  (set)      Specifies a set of one or more files. Wildcards may be used.\n  command    Specifies the command to carry out for each file.\n  command-parameters\n             Specifies parameters or switches for the specified command.\n\nTo use the command in a batch program, specify %%%%variable instead of %%variable.\n");
	MSG_Add("SHELL_CMD_LFNFOR_HELP","Enables or disables long filenames when processing FOR wildcards.\n");
	MSG_Add("SHELL_CMD_LFNFOR_HELP_LONG","LFNFOR [ON | OFF]\n\nType LFNFOR without a parameter to display the current LFNFOR setting.\n\nThis command is only useful if LFN support is currently enabled.\n");
	MSG_Add("SHELL_CMD_TYPE_HELP","Displays the contents of a text file.\n");
	MSG_Add("SHELL_CMD_TYPE_HELP_LONG","TYPE [drive:][path][filename]\n");
	MSG_Add("SHELL_CMD_REM_HELP","Adds comments in a batch file.\n");
	MSG_Add("SHELL_CMD_REM_HELP_LONG","REM [comment]\n");
	MSG_Add("SHELL_CMD_RENAME_HELP","Renames a file/directory or files.\n");
	MSG_Add("SHELL_CMD_RENAME_HELP_LONG","RENAME [drive:][path][directoryname1 | filename1] [directoryname2 | filename2]\n"
	        "REN [drive:][path][directoryname1 | filename1] [directoryname2 | filename2]\n\n"
	        "Note that you can not specify a new drive or path for your destination.\n\n"
	        "Wildcards (* and ?) are supported for files. For example, the following command\n"
	        "renames all text files: \033[37;1mREN *.TXT *.BAK\033[0m\n");
	MSG_Add("SHELL_CMD_DELETE_HELP","Removes one or more files.\n");
	MSG_Add("SHELL_CMD_DELETE_HELP_LONG","DEL [/P] [/F] [/Q] names\n"
		   "ERASE [/P] [/F] [/Q] names\n\n"
		   "  names         Specifies a list of one or more files or directories.\n"
		   "                Wildcards may be used to delete multiple files. If a\n"
		   "                directory is specified, all files within the directory\n"
		   "                will be deleted.\n"
		   "  /P            Prompts for confirmation before deleting one or more files.\n"
		   "  /F            Force deleting of read-only files.\n"
		   "  /Q            Quiet mode, do not ask if ok to delete on global wildcard.\n");
	MSG_Add("SHELL_CMD_COPY_HELP","Copies one or more files.\n");
	MSG_Add("SHELL_CMD_COPY_HELP_LONG","COPY [/Y | /-Y] source [+source [+ ...]] [destination]\n\n"
		   "  source        Specifies the file or files to be copied.\n"
		   "  destination   Specifies the directory and/or filename for the new file(s).\n"
		   "  /Y            Suppresses prompting to confirm you want to overwrite an\n"
		   "                existing destination file.\n"
		   "  /-Y           Causes prompting to confirm you want to overwrite an\n"
           "                existing destination file.\n\n"
		   "The switch /Y may be preset in the COPYCMD environment variable.\n"
		   "This may be overridden with /-Y on the command line.\n\n"
		   "To append files, specify a single file for destination, but multiple files\n"
		   "for source (using wildcards or file1+file2+file3 format).\n");
	MSG_Add("SHELL_CMD_CALL_HELP","Starts a batch file from within another batch file.\n");
	MSG_Add("SHELL_CMD_CALL_HELP_LONG","CALL [drive:][path]filename [batch-parameters]\n\n"
		   "batch-parameters   Specifies any command-line information required by\n"
		   "                   the batch program.\n");
	MSG_Add("SHELL_CMD_SUBST_HELP","Assigns an internal directory to a drive.\n");
	MSG_Add("SHELL_CMD_SUBST_HELP_LONG","SUBST [drive1: [drive2:]path]\nSUBST drive1: /D\n\n"
		   "  drive1:       Specifies a drive to which you want to assign a path.\n"
		   "  [drive2:]path Specifies a mounted local drive and path you want to assign to.\n"
		   "  /D            Deletes a mounted or substituted drive.\n\n"
		   "Type SUBST with no parameters to display a list of mounted local drives.\n");
	MSG_Add("SHELL_CMD_LOADHIGH_HELP","Loads a program into upper memory (requires XMS and UMB memory).\n");
	MSG_Add("SHELL_CMD_LOADHIGH_HELP_LONG","LH              [drive:][path]filename [parameters]\n"
		   "LOADHIGH        [drive:][path]filename [parameters]\n");
	MSG_Add("SHELL_CMD_CHOICE_HELP","Waits for a user keypress to choose one of a set of choices.\n");
	MSG_Add("SHELL_CMD_CHOICE_HELP_LONG","CHOICE [/C:choices] [/N] [/S] /T[:]c,nn text\n\n"
	        "  /C[:]choices Specifies allowable keys.  Default is: yn.\n"
	        "  /N           Do not display the choices at end of prompt.\n"
	        "  /S           Enables case-sensitive choices to be selected.\n"
	        "  /T[:]c,nn    Default choice to c after nn seconds.\n"
	        "  text         The text to display as a prompt.\n\n"
	        "ERRORLEVEL is set to offset of key user presses in choices.\n");
	MSG_Add("SHELL_CMD_ATTRIB_HELP","Displays or changes file attributes.\n");
	MSG_Add("SHELL_CMD_ATTRIB_HELP_LONG","ATTRIB [+R | -R] [+A | -A] [+S | -S] [+H | -H] [drive:][path][filename] [/S]\n\n"
			"  +   Sets an attribute.\n"
			"  -   Clears an attribute.\n"
			"  R   Read-only file attribute.\n"
			"  A   Archive file attribute.\n"
			"  S   System file attribute.\n"
			"  H   Hidden file attribute.\n"
			"  [drive:][path][filename]\n"
			"      Specifies file(s) or directory for ATTRIB to process.\n"
			"  /S  Processes files in all directories in the specified path.\n");
	MSG_Add("SHELL_CMD_PATH_HELP","Displays or sets a search path for executable files.\n");
	MSG_Add("SHELL_CMD_PATH_HELP_LONG","PATH [[drive:]path[;...][;%PATH%]\n"
		   "PATH ;\n\n"
		   "Type PATH ; to clear all search path settings.\n"
		   "Type PATH without parameters to display the current path.\n");
	MSG_Add("SHELL_CMD_PUSHD_HELP","Stores the current directory for use by the POPD command, then\nchanges to the specified directory.\n");
	MSG_Add("SHELL_CMD_PUSHD_HELP_LONG","PUSHD [path]\n\n"
	        "path        Specifies the directory to make the current directory.\n\n"
	        "Type PUSHD with no parameters to display currently stored directories.\n");
	MSG_Add("SHELL_CMD_POPD_HELP","Changes to the directory stored by the PUSHD command.\n");
	MSG_Add("SHELL_CMD_POPD_HELP_LONG","POPD\n");
	MSG_Add("SHELL_CMD_VERIFY_HELP","Controls whether to verify files are written correctly to a disk.\n");
	MSG_Add("SHELL_CMD_VERIFY_HELP_LONG","VERIFY [ON | OFF]\n\nType VERIFY without a parameter to display the current VERIFY setting.\n");
	MSG_Add("SHELL_CMD_VER_HELP","Displays or sets DOSBox-X's reported DOS version.\n");
	MSG_Add("SHELL_CMD_VER_HELP_LONG","VER [/R]\n"
		   "VER [SET] number or VER SET [major minor]\n\n"
		   "  /R                 Display DOSBox-X's Git commit version and build date.\n"
		   "  [SET] number       Set the specified number as the reported DOS version.\n"
		   "  SET [major minor]  Set the reported DOS version in major and minor format.\n\n"
		   "  \033[0mE.g., \033[37;1mVER 6.0\033[0m or \033[37;1mVER 7.1\033[0m sets the DOS version to 6.0 and 7.1, respectively.\n"
		   "  On the other hand, \033[37;1mVER SET 7 1\033[0m sets the DOS version to 7.01 instead of 7.1.\n\n"
		   "Type VER without parameters to display DOSBox-X and the reported DOS version.\n");
	MSG_Add("SHELL_CMD_VER_VER","DOSBox-X version %s (%s). Reported DOS version %d.%02d.\n");
	MSG_Add("SHELL_CMD_VER_INVALID","The specified DOS version is not correct.\n");
	MSG_Add("SHELL_CMD_VOL_HELP","Displays the disk volume label and serial number, if they exist.\n");
	MSG_Add("SHELL_CMD_VOL_HELP_LONG","VOL [drive]\n");
	MSG_Add("SHELL_CMD_PROMPT_HELP","Changes the command prompt.\n");
	MSG_Add("SHELL_CMD_PROMPT_HELP_LONG","PROMPT [text]\n"
		   "  text    Specifies a new command prompt.\n\n"
		   "Prompt can be made up of normal characters and the following special codes:\n"
		   "  $A   & (Ampersand)\n"
		   "  $B   | (pipe)\n"
		   "  $C   ( (Left parenthesis)\n"
		   "  $D   Current date\n"
		   "  $E   Escape code (ASCII code 27)\n"
		   "  $F   ) (Right parenthesis)\n"
		   "  $G   > (greater-than sign)\n"
		   "  $H   Backspace (erases previous character)\n"
		   "  $L   < (less-than sign)\n"
		   "  $N   Current drive\n"
		   "  $P   Current drive and path\n"
		   "  $Q   = (equal sign)\n"
		   "  $S     (space)\n"
		   "  $T   Current time\n"
		   "  $V   DOS version number\n"
		   "  $_   Carriage return and linefeed\n"
		   "  $$   $ (dollar sign)\n");
    MSG_Add("SHELL_CMD_ALIAS_HELP", "Defines or displays aliases.\n");
    MSG_Add("SHELL_CMD_ALIAS_HELP_LONG", "ALIAS [name[=value] ... ]\n\nType ALIAS without parameters to display the list of aliases in the form:\n`ALIAS NAME = VALUE'\n");
    MSG_Add("SHELL_CMD_ASSOC_HELP", "Displays or changes file extension associations.\n");
    MSG_Add("SHELL_CMD_ASSOC_HELP_LONG", "ASSOC [.ext[=command] ... ]\n\nType ASSOC without parameters to display the current file associations.\nFile extensions must start with a dot (.); wildcards (* and ?) are allowed.\n");
	MSG_Add("SHELL_CMD_CHCP_HELP", "Displays or changes the current DOS code page.\n");
	MSG_Add("SHELL_CMD_CHCP_HELP_LONG", "CHCP [nnn [file]]\nCHCP nnn[:[language]]\n\n  nnn      Specifies a code page number.\n  file     Specifies a code page file.\n  language Specifies a corresponding language.\n\nSupported code pages for changing in the TrueType font output:\n\n437,737,775,808,850,852,853,855-869,872,874,1250-1258,3021\n\nAlso double-byte code pages including 932, 936, 949, and 950/951.\n\nCustomized code pages are supported by providing code page files.\n");
	MSG_Add("SHELL_CMD_CHCP_ACTIVE", "Active code page: %d\n");
	MSG_Add("SHELL_CMD_CHCP_MISSING", "ASCII characters not defined in TTF font: %d\n");
	MSG_Add("SHELL_CMD_CHCP_INVALID", "Invalid code page number - %s\n");
	MSG_Add("SHELL_CMD_COUNTRY_HELP", "Displays or changes the current country.\n");
	MSG_Add("SHELL_CMD_COUNTRY_HELP_LONG", "COUNTRY [nnn] \n\n  nnn   Specifies a country code.\n\nCountry-specific information such as date and time formats will be affected.\n");
    MSG_Add("SHELL_CMD_CTTY_HELP","Changes the terminal device used to control the system.\n");
	MSG_Add("SHELL_CMD_CTTY_HELP_LONG","CTTY device\n  device        The terminal device to use, such as CON.\n");
	MSG_Add("SHELL_CMD_MORE_HELP","Displays output one screen at a time.\n");
	MSG_Add("SHELL_CMD_MORE_HELP_LONG","MORE [drive:][path][filename]\nMORE < [drive:][path]filename\ncommand-name | MORE [drive:][path][filename]\n");
	MSG_Add("SHELL_CMD_TRUENAME_HELP","Finds the fully-expanded name for a file.\n");
	MSG_Add("SHELL_CMD_TRUENAME_HELP_LONG","TRUENAME [/H] file\n");
	MSG_Add("SHELL_CMD_DXCAPTURE_HELP","Runs program with video or audio capture.\n");
	MSG_Add("SHELL_CMD_DXCAPTURE_HELP_LONG","DX-CAPTURE [/V|/-V] [/A|/-A] [/M|/-M] [command] [options]\n\nIt will start video or audio capture, run program, and then automatically stop capture when the program exits.\n");
#if C_DEBUG
	MSG_Add("SHELL_CMD_DEBUGBOX_HELP","Runs program and breaks into debugger at entry point.\n");
	MSG_Add("SHELL_CMD_DEBUGBOX_HELP_LONG","DEBUGBOX [command] [options]\n\nType DEBUGBOX without a parameter to start the debugger.\n");
#endif
	MSG_Add("SHELL_CMD_COMMAND_HELP","Starts the DOSBox-X command shell.\n\nThe following options are accepted:\n\n  /C    Executes the specified command and returns.\n  /K    Executes the specified command and continues running.\n  /P    Loads a permanent copy of the command shell.\n  /INIT Initializes the command shell.\n");

	/* Regular startup */
	call_shellstop=CALLBACK_Allocate();
	/* Setup the startup CS:IP to kill the last running machine when exited */
	RealPt newcsip=CALLBACK_RealPointer(call_shellstop);
	SegSet16(cs,RealSeg(newcsip));
	reg_ip=RealOff(newcsip);

	CALLBACK_Setup(call_shellstop,shellstop_handler,CB_IRET,"shell stop");

    /* NTS: Some DOS programs behave badly if run from a command interpreter
     *      who's PSP segment is too low in memory and does not appear in
     *      the MCB chain (SimCity 2000). So allocate shell memory normally
     *      as any DOS application would do.
     *
     *      That includes allocating COMMAND.COM stack NORMALLY (not up in
     *      the UMB as DOSBox SVN would do) */

	/* Now call up the shell for the first time */
	uint16_t psp_seg;//=DOS_FIRST_SHELL;
	uint16_t env_seg;//=DOS_FIRST_SHELL+19; //DOS_GetMemory(1+(4096/16))+1;
	uint16_t stack_seg;//=DOS_GetMemory(2048/16,"COMMAND.COM stack");
    uint16_t tmp,total_sz;

    // decide shell env size
    if (dosbox_shell_env_size == 0)
        dosbox_shell_env_size = (0x158u - (0x118u + 19u)) << 4u; /* equivalent to mainline DOSBox */
    else
        dosbox_shell_env_size = (dosbox_shell_env_size+15u)&(~15u); /* round up to paragraph */

    LOG_MSG("COMMAND.COM env size:             %u bytes",dosbox_shell_env_size);

    // According to some sources, 0x0008 is a special PSP segment value used by DOS before the first
    // program is used. We need the current PSP segment to be nonzero so that DOS_AllocateMemory()
    // can properly allocate memory.
    dos.psp(8);

    auto savedMemAllocStrategy = DOS_GetMemAllocStrategy();
    auto shellHigh = std::string(static_cast<Section_prop*>(control->GetSection("dos"))->Get_string("shellhigh"));
    if (shellHigh=="true" || shellHigh=="1" ||
        (shellHigh=="auto" && dos.version.major >= 7))
    {
	    DOS_SetMemAllocStrategy(savedMemAllocStrategy | 0x80);
    }

    // COMMAND.COM environment block
    tmp = dosbox_shell_env_size>>4;
	if (!DOS_AllocateMemory(&env_seg,&tmp)) E_Exit("COMMAND.COM failed to allocate environment block segment");
    LOG_MSG("COMMAND.COM environment block:    0x%04x sz=0x%04x",env_seg,tmp);

    // COMMAND.COM main binary (including PSP and stack)
    tmp = 0x1A + (2048/16);
    total_sz = tmp;
	if (!DOS_AllocateMemory(&psp_seg,&tmp)) E_Exit("COMMAND.COM failed to allocate main body + PSP segment");
    LOG_MSG("COMMAND.COM main body (PSP):      0x%04x sz=0x%04x",psp_seg,tmp);

    DOS_SetMemAllocStrategy(savedMemAllocStrategy);

    // now COMMAND.COM has a main body and PSP segment, reflect it
    dos.psp(psp_seg);
    shell_psp = psp_seg;

    {
        DOS_MCB mcb((uint16_t)(env_seg-1));
        mcb.SetPSPSeg(psp_seg);
        mcb.SetFileName("COMMAND");
    }

    {
        DOS_MCB mcb((uint16_t)(psp_seg-1));
        mcb.SetPSPSeg(psp_seg);
        mcb.SetFileName("COMMAND");
    }

    // set the stack at 0x1A
    stack_seg = psp_seg + 0x1A;
    LOG_MSG("COMMAND.COM stack:                0x%04x",stack_seg);

    // set the stack pointer
	SegSet16(ss,stack_seg);
	reg_sp=2046;

	/* Set up int 24 and psp (Telarium games) */
	real_writeb(psp_seg+16+1,0,0xea);		/* far jmp */
	real_writed(psp_seg+16+1,1,real_readd(0,0x24*4));
	real_writed(0,0x24*4,((uint32_t)psp_seg<<16) | ((16+1)<<4));

	/* Set up int 23 to "int 20" in the psp. Fixes what.exe */
	real_writed(0,0x23*4,((uint32_t)psp_seg<<16));

	/* Set up int 2e handler */
    if (call_int2e == 0)
        call_int2e = CALLBACK_Allocate();

    //	RealPt addr_int2e=RealMake(psp_seg+16+1,8);
    // NTS: It's apparently common practice to enumerate MCBs by reading the segment value of INT 2Eh and then
    //      scanning forward from there. The assumption seems to be that COMMAND.COM writes INT 2Eh there using
    //      it's PSP segment and an offset like that of a COM executable even though COMMAND.COM is often an EXE file.
	RealPt addr_int2e=RealMake(psp_seg,8+((16+1)*16));

    CALLBACK_Setup(call_int2e,&INT2E_Handler,CB_IRET_STI,Real2Phys(addr_int2e),"Shell Int 2e");
	RealSetVec(0x2e,addr_int2e);

	/* Setup environment */
	PhysPt env_write=PhysMake(env_seg,0);
	MEM_BlockWrite(env_write,path_string,(Bitu)(strlen(path_string)+1));
	env_write += (PhysPt)(strlen(path_string)+1);
	MEM_BlockWrite(env_write,comspec_string,(Bitu)(strlen(comspec_string)+1));
	env_write += (PhysPt)(strlen(comspec_string)+1);
	MEM_BlockWrite(env_write,prompt_string,(Bitu)(strlen(prompt_string)+1));
	env_write +=(PhysPt)(strlen(prompt_string)+1);
	mem_writeb(env_write++,0);
	mem_writew(env_write,1);
	env_write+=2;
	MEM_BlockWrite(env_write,full_name,(Bitu)(strlen(full_name)+1));

//    extern bool Mouse_Vertical;
	extern bool Mouse_Drv;
	Mouse_Drv = true;

	DOS_PSP psp(psp_seg);
	psp.MakeNew(0);
	dos.psp(psp_seg);
   
	/* The start of the filetable in the psp must look like this:
	 * 01 01 01 00 02
	 * In order to achieve this: First open 2 files. Close the first and
	 * duplicate the second (so the entries get 01) */
	uint16_t dummy=0;
	DOS_OpenFile("CON",OPEN_READWRITE,&dummy);	/* STDIN  */
	DOS_OpenFile("CON",OPEN_READWRITE,&dummy);	/* STDOUT */
	DOS_CloseFile(0);							/* Close STDIN */
	DOS_ForceDuplicateEntry(1,0);				/* "new" STDIN */
	DOS_ForceDuplicateEntry(1,2);				/* STDERR */
	DOS_OpenFile("CON",OPEN_READWRITE,&dummy);	/* STDAUX */
	if (!DOS_OpenFile("PRN",OPEN_READWRITE,&dummy)) DOS_OpenFile("CON",OPEN_READWRITE,&dummy);	/* STDPRN */

    psp.SetSize(psp_seg + total_sz);
    psp.SetStack(((unsigned int)stack_seg << 16u) + (unsigned int)reg_sp);

	/* Create appearance of handle inheritance by first shell */
	for (uint16_t i=0;i<5;i++) {
		uint8_t handle=psp.GetFileHandle(i);
		if (Files[handle]) Files[handle]->AddRef();
	}

	psp.SetParent(psp_seg);
	/* Set the environment */
	psp.SetEnvironment(env_seg);
	/* Set the command line for the shell start up */
	CommandTail tail;
	tail.count=(uint8_t)strlen(init_line);
	memset(&tail.buffer, 0, CTBUF);
	strncpy(tail.buffer,init_line,CTBUF);
	MEM_BlockWrite(PhysMake(psp_seg,CTBUF+1),&tail,CTBUF+1);
	
	/* Setup internal DOS Variables */
	dos.dta(RealMake(psp_seg,CTBUF+1));
	dos.psp(psp_seg);
}

/* Pfff... starting and running the shell from a configuration section INIT
 * What the hell were you guys thinking? --J.C. */
void SHELL_Run() {
	dos_shell_running_program = false;
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
	Reflect_Menu();
#endif

	LOG(LOG_MISC,LOG_DEBUG)("Running DOS shell now");

	if (first_shell != NULL) E_Exit("Attempt to start shell when shell already running");
	Section_prop *section = static_cast<Section_prop *>(control->GetSection("config"));
	bool altshell=false;
	char namestr[CROSS_LEN], tmpstr[CROSS_LEN], *name=namestr, *tmp=tmpstr;
	SHELL_ProgramStart_First_shell(&first_shell);
	first_shell->Prepare();
	prepared = true;
	if (section!=NULL&&!control->opt_noconfig&&!control->opt_securemode&&!control->SecureMode()) {
		char *shell = (char *)section->Get_string("shell");
		if (strlen(shell)) {
			tmp=trim(shell);
			name=StripArg(tmp);
			upcase(name);
			if (*name&&(DOS_FileExists(name)||DOS_FileExists((std::string("Z:\\SYSTEM\\")+name).c_str())||DOS_FileExists((std::string("Z:\\BIN\\")+name).c_str())||DOS_FileExists((std::string("Z:\\DOS\\")+name).c_str())||DOS_FileExists((std::string("Z:\\4DOS\\")+name).c_str())||DOS_FileExists((std::string("Z:\\DEBUG\\")+name).c_str())||DOS_FileExists((std::string("Z:\\TEXTUTIL\\")+name).c_str()))) {
				strreplace(name,'/','\\');
				altshell=true;
			} else if (*name)
				first_shell->WriteOut(MSG_Get("SHELL_MISSING_FILE"), name);
		}
	}
	if (control->opt_test) {
#if C_DEBUG
		testerr = RUN_ALL_TESTS();
		control->opt_nolog = false;
		LOG_MSG("Unit test completed: %s\n", testerr?"failure":"success");
		control->opt_nolog = true;
#else
		testerr = true;
		printf("Unit tests are only available in debug builds\n\n");
#endif
#if defined(WIN32)
		if (sdl_wait_on_error()) DOSBox_ConsolePauseWait();
#endif
		return;
	}
	i4dos=false;
	if (altshell) {
		if (strstr(name, "4DOS.COM")) i4dos=true;
		first_shell->SetEnv("COMSPEC",name);
		if (!strlen(tmp)) {
			char *p=strrchr(name, '\\');
			if (!strcasecmp(p==NULL?name:p+1, "COMMAND.COM") || !strcasecmp(name, "Z:COMMAND.COM")) {strcpy(tmpstr, init_line);tmp=tmpstr;}
			else if (!strcasecmp(p==NULL?name:p+1, "4DOS.COM") || !strcasecmp(name, "Z:4DOS.COM")) {strcpy(tmpstr, "AUTOEXEC.BAT");tmp=tmpstr;}
		}
		first_shell->Execute(name, tmp);
		return;
	}
	try {
		first_shell->Run();
		delete first_shell;
		first_shell = 0;//Make clear that it shouldn't be used anymore
		prepared = false;
		dos_shell_running_program = false;
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
		Reflect_Menu();
#endif
	}
	catch (...) {
		delete first_shell;
		first_shell = 0;//Make clear that it shouldn't be used anymore
		prepared = false;
		dos_shell_running_program = false;
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
		Reflect_Menu();
#endif
		throw;
	}
}
