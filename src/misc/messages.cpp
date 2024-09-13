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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dosbox.h"
#include "cross.h"
#include "logging.h"
#include "support.h"
#include "setup.h"
#include "render.h"
#include "control.h"
#include "shell.h"
#include "menu.h"
#include "jfont.h"
#include "mapper.h"
#include <map>
#include <list>
#include <string>
#if defined(__MINGW32__) && !defined(HX_DOS)
#include <imm.h>
#endif
using namespace std;

int msgcodepage = 0, lastmsgcp = 0;
bool morelen = false, inmsg = false, loadlang = false;
bool uselangcp = false; // True if Language file is loaded via CONFIG -set langcp option. Use codepage specified in the language file 
bool isSupportedCP(int newCP), CodePageHostToGuestUTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/), CodePageGuestToHostUTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/), systemmessagebox(char const * aTitle, char const * aMessage, char const * aDialogType, char const * aIconType, int aDefaultButton), OpenGL_using(void);
void InitFontHandle(void), ShutFontHandle(void), refreshExtChar(void), SetIME(void), runRescan(const char *str), menu_update_dynamic(void), menu_update_autocycle(void), update_bindbutton_text(void), set_eventbutton_text(const char *eventname, const char *buttonname), JFONT_Init(), DOSBox_SetSysMenu(), UpdateSDLDrawTexture(), makestdcp950table(), makeseacp951table();
std::string langname = "", langnote = "", GetDOSBoxXPath(bool withexe=false);
extern int lastcp, FileDirExistUTF8(std::string &localname, const char *name), toSetCodePage(DOS_Shell *shell, int newCP, int opt);
extern bool dos_kernel_disabled, force_conversion, showdbcs, dbcs_sbcs, enableime, tonoime, chinasea;
extern uint16_t GetDefaultCP();
extern const char * RunningProgram;
Bitu DOS_ChangeKeyboardLayout(const char* layoutname, int32_t codepage);
Bitu DOS_ChangeCodepage(int32_t codepage, const char* codepagefile);
Bitu DOS_LoadKeyboardLayout(const char* layoutname, int32_t codepage, const char* codepagefile);

#define LINE_IN_MAXLEN 2048

struct MessageBlock {
	string name;
	string val;
	MessageBlock(const char* _name, const char* _val):
	name(_name),val(_val){}
};

static list<MessageBlock> Lang;
typedef list<MessageBlock>::iterator itmb;

void MSG_Add(const char * _name, const char* _val) {
	/* Find the message */
	for(itmb tel=Lang.begin();tel!=Lang.end();++tel) {
		if((*tel).name==_name) { 
//			LOG_MSG("double entry for %s",_name); //Message file might be loaded before default text messages
			return;
		}
	}
	/* if the message doesn't exist add it */
	Lang.emplace_back(MessageBlock(_name,_val));
}

void MSG_Replace(const char * _name, const char* _val) {
	/* Find the message */
	for(itmb tel=Lang.begin();tel!=Lang.end();++tel) {
		if((*tel).name==_name) { 
			Lang.erase(tel);
			break;
		}
	}
	/* Even if the message doesn't exist add it */
	Lang.emplace_back(MessageBlock(_name,_val));
}

bool InitCodePage() {
    if (!dos.loaded_codepage || dos_kernel_disabled || force_conversion) {
        if (((control->opt_langcp && msgcodepage != dos.loaded_codepage) || uselangcp) && msgcodepage>0 && isSupportedCP(msgcodepage)) {
            dos.loaded_codepage = msgcodepage;
            return true;
        }
        Section_prop *section = static_cast<Section_prop *>(control->GetSection("config"));
        if (section!=NULL && !control->opt_noconfig && !IS_PC98_ARCH && !IS_JEGA_ARCH && !IS_DOSV) {
            char *countrystr = (char *)section->Get_string("country"), *r=strchr(countrystr, ',');
            if (r!=NULL && *(r+1)) {
                int cp = atoi(trim(r+1));
                if (cp>0 && isSupportedCP(cp)) {
                    dos.loaded_codepage = cp;
                    return true;
                }
            }
        }
        if (msgcodepage>0) {
            dos.loaded_codepage = msgcodepage;
            return true;
        }
    }
    if (!dos.loaded_codepage) {
        dos.loaded_codepage = GetDefaultCP();
        return false;
    } else
        return true;
}

void AddMessages() {
    MSG_Add("AUTOEXEC_CONFIGFILE_HELP",
        "Lines in this section will be run at startup.\n"
        "You can put your MOUNT lines here.\n"
    );
    MSG_Add("CONFIGFILE_INTRO",
            "# This is the configuration file for DOSBox-X %s. (Please use the latest version of DOSBox-X)\n"
            "# Lines starting with a # are comment lines and are ignored by DOSBox-X.\n"
            "# They are used to (briefly) document the effect of each option.\n"
        "# To write out ALL options, use command 'config -all' with -wc or -writeconf options.\n");
    MSG_Add("CONFIG_SUGGESTED_VALUES", "Possible values");
    MSG_Add("CONFIG_ADVANCED_OPTION", "Advanced options (see full configuration reference file [dosbox-x.reference.full.conf] for more details)");
    MSG_Add("CONFIG_TOOL","DOSBox-X Configuration Tool");
    MSG_Add("CONFIG_TOOL_EXIT","Exit configuration tool");
    MSG_Add("MAPPER_EDITOR_EXIT","Exit mapper editor");
    MSG_Add("SAVE_MAPPER_FILE","Save mapper file");
    MSG_Add("WARNING","Warning");
    MSG_Add("YES","Yes");
    MSG_Add("NO","No");
    MSG_Add("OK","OK");
    MSG_Add("CANCEL","Cancel");
    MSG_Add("CLOSE","Close");
    MSG_Add("DEBUGCMD","Enter Debugger Command");
    MSG_Add("ADD","Add");
    MSG_Add("DEL","Del");
    MSG_Add("NEXT","Next");
    MSG_Add("SAVE","Save");
    MSG_Add("EXIT","Exit");
    MSG_Add("CAPTURE","Capture");
    MSG_Add("SAVE_CONFIGURATION","Save configuration");
    MSG_Add("SAVE_LANGUAGE","Save language file");
    MSG_Add("SAVE_RESTART","Save & Restart");
    MSG_Add("PASTE_CLIPBOARD","Paste Clipboard");
    MSG_Add("APPEND_HISTORY","Append History");
    MSG_Add("EXECUTE_NOW","Execute Now");
    MSG_Add("ADDITION_CONTENT","Additional Content:");
    MSG_Add("CONTENT","Content:");
    MSG_Add("EDIT_FOR","Edit %s");
    MSG_Add("HELP_FOR","Help for %s");
    MSG_Add("SELECT_VALUE", "Select property value");
    MSG_Add("CONFIGURATION","Configuration");
    MSG_Add("SETTINGS","Settings");
    MSG_Add("LOGGING_OUTPUT","DOSBox-X logging output");
    MSG_Add("CODE_OVERVIEW","Code overview");
    MSG_Add("VISIT_HOMEPAGE","Visit Homepage");
    MSG_Add("GET_STARTED","Getting Started");
    MSG_Add("CDROM_SUPPORT","CD-ROM Support");
    MSG_Add("DRIVE_INFORMATION","Drive information");
    MSG_Add("MOUNTED_DRIVE_NUMBER","Mounted drive numbers");
    MSG_Add("IDE_CONTROLLER_ASSIGNMENT","IDE controller assignment");
    MSG_Add("HELP_COMMAND","Help on DOS command");
    MSG_Add("CURRENT_VOLUME","Current sound mixer volumes");
    MSG_Add("CURRENT_SBCONFIG","Sound Blaster configuration");
    MSG_Add("CURRENT_MIDICONFIG","Current MIDI configuration");
    MSG_Add("CREATE_IMAGE","Create blank disk image");
    MSG_Add("NETWORK_LIST","Network interface list");
    MSG_Add("PRINTER_LIST","Printer device list");
    MSG_Add("INTRODUCTION","Introduction");
    MSG_Add("SHOW_ADVOPT", "Show advanced options");
    MSG_Add("USE_PRIMARYCONFIG", "Use primary config file");
    MSG_Add("USE_PORTABLECONFIG", "Use portable config file");
    MSG_Add("USE_USERCONFIG", "Use user config file");
    MSG_Add("CONFIG_SAVETO", "Enter filename for the configuration file to save to:");
    MSG_Add("CONFIG_SAVEALL", "Save all (including advanced) config options to the configuration file");
    MSG_Add("LANG_FILENAME", "Enter filename for language file:");
    MSG_Add("LANG_LANGNAME", "Language name (optional):");
    MSG_Add("INTRO_MESSAGE", "Welcome to DOSBox-X, a free and complete DOS emulation package.\nDOSBox-X creates a DOS shell which looks like the plain DOS.\nYou can also run Windows 3.x and 9x/Me inside the DOS machine.");
    MSG_Add("DRIVE","Drive");
    MSG_Add("TYPE","Type");
    MSG_Add("LABEL","Label");
    MSG_Add("DRIVE_NUMBER","Drive number");
    MSG_Add("DISK_NAME","Disk name");
    MSG_Add("IDE_POSITION","IDE position");
    MSG_Add("SWAP_SLOT","Swap slot");
    MSG_Add("EMPTY_SLOT","Empty slot");
    MSG_Add("SLOT","Slot");
    MSG_Add("SELECT_EVENT", "Select an event to change.");
    MSG_Add("SELECT_DIFFERENT_EVENT", "Select an event or press Add/Del/Next buttons.");
    MSG_Add("PRESS_JOYSTICK_KEY", "Press a key/joystick button or move the joystick.");
    MSG_Add("CAPTURE_ENABLED", "Capture enabled. Hit ESC to release capture.");
    MSG_Add("MAPPER_FILE_SAVED", "Mapper file saved");
    MSG_Add("AUTO_CYCLE_MAX","Auto cycles [max]");
    MSG_Add("AUTO_CYCLE_AUTO","Auto cycles [auto]");
    MSG_Add("AUTO_CYCLE_OFF","Auto cycles [off]");
}

void MSG_Init(void);
void SetKEYBCP() {
    if (IS_PC98_ARCH || IS_JEGA_ARCH || IS_DOSV || dos_kernel_disabled || !strcmp(RunningProgram, "LOADLIN")) return;
    Bitu return_code;

    if(msgcodepage == 932 || msgcodepage == 936 || msgcodepage == 949 || msgcodepage == 950 || msgcodepage == 951) {
        MSG_Init();
        InitFontHandle();
        JFONT_Init();
        dos.loaded_codepage = msgcodepage;
    }
    else {
        return_code = DOS_ChangeCodepage(858, "auto"); /* FIX_ME: Somehow requires to load codepage twice */
        return_code = DOS_ChangeCodepage(msgcodepage, "auto");
        if(return_code == KEYB_NOERROR) {
            dos.loaded_codepage = msgcodepage;
        }
    }
    runRescan("-A -Q");
}

FILE *testLoadLangFile(const char *fname) {
    std::string config_path, res_path, exepath=GetDOSBoxXPath();
    Cross::GetPlatformConfigDir(config_path), Cross::GetPlatformResDir(res_path);
	FILE * mfile=fopen(fname,"rt");
	if (!mfile) mfile=fopen((fname + std::string(".lng")).c_str(),"rt");
	if (!mfile && exepath.size()) mfile=fopen((exepath + fname).c_str(),"rt");
	if (!mfile && exepath.size()) mfile=fopen((exepath + fname + ".lng").c_str(),"rt");
	if (!mfile && config_path.size()) mfile=fopen((config_path + fname).c_str(),"rt");
	if (!mfile && config_path.size()) mfile=fopen((config_path + fname + ".lng").c_str(),"rt");
	if (!mfile && res_path.size()) mfile=fopen((res_path + fname).c_str(),"rt");
	if (!mfile && res_path.size()) mfile=fopen((res_path + fname + ".lng").c_str(),"rt");
	if (!mfile) mfile=fopen((std::string("languages/") + fname).c_str(),"rt");
	if (!mfile) mfile=fopen((std::string("languages/") + fname + ".lng").c_str(),"rt");
	if (!mfile && exepath.size()) mfile=fopen((exepath + "languages/" + fname).c_str(),"rt");
	if (!mfile && exepath.size()) mfile=fopen((exepath + "languages/" + fname + ".lng").c_str(),"rt");
	if (!mfile && config_path.size()) mfile=fopen((config_path + "languages/" + fname).c_str(),"rt");
	if (!mfile && config_path.size()) mfile=fopen((config_path + "languages/" + fname + ".lng").c_str(),"rt");
	if (!mfile && res_path.size()) mfile=fopen((res_path + "languages/" + fname).c_str(),"rt");
	if (!mfile && res_path.size()) mfile=fopen((res_path + "languages/" + fname + ".lng").c_str(),"rt");
	if (!mfile) mfile=fopen((std::string("language/") + fname).c_str(),"rt");
	if (!mfile) mfile=fopen((std::string("language/") + fname + ".lng").c_str(),"rt");
	if (!mfile && exepath.size()) mfile=fopen((exepath + "language/" + fname).c_str(),"rt");
	if (!mfile && exepath.size()) mfile=fopen((exepath + "language/" + fname + ".lng").c_str(),"rt");
	if (!mfile && config_path.size()) mfile=fopen((config_path + "language/" + fname).c_str(),"rt");
	if (!mfile && config_path.size()) mfile=fopen((config_path + "language/" + fname + ".lng").c_str(),"rt");
	if (!mfile && res_path.size()) mfile=fopen((res_path + "language/" + fname).c_str(),"rt");
	if (!mfile && res_path.size()) mfile=fopen((res_path + "language/" + fname + ".lng").c_str(),"rt");
#if defined(WIN32) && defined(C_SDL2)
    std::string localname = fname;
    if (!mfile && FileDirExistUTF8(localname, fname) == 1) mfile=fopen(localname.c_str(),"rt");
#endif
    return mfile;
}

char loaded_fname[LINE_IN_MAXLEN + 1024];
void LoadMessageFile(const char * fname) {
	if (!fname) return;
	if(*fname=='\0') return;//empty string=no languagefile
    if (!strcmp(fname, loaded_fname)){
        //LOG_MSG("Message file %s already loaded.",fname);
        return;
    }
    strcpy(loaded_fname, fname);
	LOG(LOG_MISC,LOG_DEBUG)("Loading message file %s",loaded_fname);

	FILE * mfile=testLoadLangFile(fname);
	/* This should never happen and since other modules depend on this use a normal printf */
	if (!mfile) {
		std::string message="Could not load language message file '"+std::string(fname)+"'. The default language will be used.";
		systemmessagebox("Warning", message.c_str(), "ok","warning", 1);
		SetVal("dosbox", "language", "");
		LOG_MSG("MSG:Cannot load language file: %s",fname);
		control->opt_lang = "";
		return;
	}
    langname = langnote = "";
	char linein[LINE_IN_MAXLEN+1024];
	char name[LINE_IN_MAXLEN+1024], menu_name[LINE_IN_MAXLEN], mapper_name[LINE_IN_MAXLEN];
	char string[LINE_IN_MAXLEN*10], temp[4096];
	/* Start out with empty strings */
	name[0]=0;string[0]=0;
	morelen=inmsg=true;
    bool res=true;
    bool loadlangcp = false;
    int cp=dos.loaded_codepage;
    if (!dos.loaded_codepage) res=InitCodePage();
	while(fgets(linein, LINE_IN_MAXLEN+1024, mfile)) {
		/* Parse the read line */
		/* First remove characters 10 and 13 from the line */
		char * parser=linein;
		char * writer=linein;

		while (*parser) {
			if (*parser != 10 && *parser != 13)
				*writer++ = *parser;

			parser++;
		}
		*writer=0;
		/* New string name */
		if (linein[0]==':') {
            string[0]=0;
            if (!strncasecmp(linein+1, "DOSBOX-X:", 9)) {
                char *p = linein+10;
                char *r = strchr(p, ':');
                if (*p && r!=NULL && r>p && *(r+1)) {
                    *r=0;
                    if (!strcmp(p, "CODEPAGE")) {
                        int c = atoi(r+1);
                        if(!isSupportedCP(c)) {
                            LOG_MSG("Language file: Invalid codepage :DOSBOX-X:CODEPAGE:%d", c);
                            loadlangcp = false;
                        }
                        else if (((IS_PC98_ARCH||IS_JEGA_ARCH) && c!=437 && c!=932 && !systemmessagebox("DOSBox-X language file", "You have specified a language file which uses a code page incompatible with the Japanese PC-98 or JEGA/AX system.\n\nAre you sure to use the language file for this machine type?", "yesno","question", 2)) || (((IS_JDOSV && c!=932) || (IS_PDOSV && c!=936) || (IS_KDOSV && c!=949) || (IS_TDOSV && c!=950 && c!=951)) && c!=437 && !systemmessagebox("DOSBox-X language file", "You have specified a language file which uses a code page incompatible with the current DOS/V system.\n\nAre you sure to use the language file for this system type?", "yesno","question", 2))) {
                                fclose(mfile);
                                dos.loaded_codepage = cp;
                                return;
                        }
                        else {
                            std::string msg = "The specified language file uses code page " + std::to_string(c) + ". Do you want to change to this code page accordingly?";
                            if(c != dos.loaded_codepage && (control->opt_langcp || uselangcp || !loadlang || (loadlang && systemmessagebox("DOSBox-X language file", msg.c_str(), "yesno", "question", 1)))){
                                loadlangcp = true;
                                msgcodepage = c;
                                dos.loaded_codepage = c;
                                if (c == 950 && !chinasea) makestdcp950table();
                                if (c == 951 && chinasea) makeseacp951table();
                                lastmsgcp = c;
                            }
                        }
                    } else if (!strcmp(p, "LANGUAGE"))
                        langname = r+1;
                    else if (!strcmp(p, "REMARK"))
                        langnote = r+1;
                    *r=':';
                }
            } else if (!strncasecmp(linein+1, "MENU:", 5)&&strlen(linein+6)<LINE_IN_MAXLEN) {
                *name=0;
                *mapper_name=0;
                strcpy(menu_name,linein+6);
            } else if (!strncasecmp(linein+1, "MAPPER:", 7)&&strlen(linein+8)<LINE_IN_MAXLEN) {
                *name=0;
                *menu_name=0;
                strcpy(mapper_name,linein+8);
            } else {
                *menu_name=0;
                *mapper_name=0;
                strcpy(name,linein+1);
            }
		/* End of string marker */
		} else if (linein[0]=='.'&&!strlen(trim(linein+1))) {
			/* Replace/Add the string to the internal languagefile */
			/* Remove last newline (marker is \n.\n) */
			size_t ll = strlen(string);
			if(ll && string[ll - 1] == '\n') string[ll - 1] = 0; //Second if should not be needed, but better be safe.
            if (strlen(name))
                MSG_Replace(name,string);
            else if (strlen(menu_name)>6&&!strncmp(menu_name, "drive_", 6))
                for (char c='A'; c<='Z'; c++) {
                    std::string mname = "drive_"+std::string(1, c)+(menu_name+5);
                    if (mainMenu.item_exists(mname)) mainMenu.get_item(mname).set_text(string);
                }
            else if (strlen(menu_name)&&mainMenu.item_exists(menu_name)) {
                mainMenu.get_item(menu_name).set_text(string);
                if (strlen(menu_name)>7&&!strncmp(menu_name, "mapper_", 7)) set_eventbutton_text(menu_name+7, string);
            }
            else if (strlen(mapper_name))
                set_eventbutton_text(mapper_name, string);
		} else {
			/* Normal string to be added */
            if (!CodePageHostToGuestUTF8(temp,linein))
                strcat(string,linein);
            else
                strcat(string,temp);
			strcat(string,"\n");
		}
	}
	morelen=inmsg=false;
	fclose(mfile);
    menu_update_dynamic();
    menu_update_autocycle();
    update_bindbutton_text();
    dos.loaded_codepage=cp;
    if (loadlangcp && msgcodepage>0 && isSupportedCP(msgcodepage) && msgcodepage != dos.loaded_codepage) {
        ShutFontHandle();
        if(msgcodepage == 932 || msgcodepage == 936 || msgcodepage == 949 || msgcodepage == 950 || msgcodepage == 951) {
            dos.loaded_codepage = msgcodepage;
            InitFontHandle();
            JFONT_Init();
        }
        if (!IS_DOSV && !IS_JEGA_ARCH) {
#if defined(USE_TTF)
            if (ttf.inUse) toSetCodePage(NULL, msgcodepage, -2); else
#endif
            {
                dos.loaded_codepage = msgcodepage;
                DOSBox_SetSysMenu();
#if C_OPENGL && DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
                if (OpenGL_using() && control->opt_lang.size() && lastcp && lastcp != dos.loaded_codepage)
                    UpdateSDLDrawTexture();
#endif
            }
            SetKEYBCP();
        }
    }
    refreshExtChar();
    LOG_MSG("LoadMessageFile: Loaded language file: %s",fname);
    loadlang = true;
}

const char * MSG_Get(char const * msg) {
	for(itmb tel=Lang.begin();tel!=Lang.end();++tel){
		if((*tel).name==msg)
		{
			return  (*tel).val.c_str();
		}
	}
	return msg;
}

bool MSG_Write(const char * location, const char * name) {
    char temp[4096];
	FILE* out=fopen(location,"w+t");
	if(out==NULL) return false;//maybe an error?
	if (name!=NULL) langname=std::string(name);
	if (langname!="") fprintf(out,":DOSBOX-X:LANGUAGE:%s\n",langname.c_str());
	if (dos.loaded_codepage) fprintf(out,":DOSBOX-X:CODEPAGE:%d\n",dos.loaded_codepage);
	fprintf(out,":DOSBOX-X:VERSION:%s\n",VERSION);
	fprintf(out,":DOSBOX-X:REMARK:%s\n",langnote.c_str());
	morelen=inmsg=true;
	for(itmb tel=Lang.begin();tel!=Lang.end();++tel){
        if (!CodePageGuestToHostUTF8(temp,(*tel).val.c_str()))
            fprintf(out,":%s\n%s\n.\n",(*tel).name.c_str(),(*tel).val.c_str());
        else
            fprintf(out,":%s\n%s\n.\n",(*tel).name.c_str(),temp);
	}
	std::vector<DOSBoxMenu::item> master_list = mainMenu.get_master_list();
	for (auto &id : master_list) {
		if (id.is_allocated()&&id.get_type()!=DOSBoxMenu::separator_type_id&&id.get_type()!=DOSBoxMenu::vseparator_type_id&&!(id.get_name().size()==5&&id.get_name().substr(0,4)=="slot")&&!(id.get_name().size()>9&&id.get_name().substr(0,8)=="command_")&&!(id.get_name().size()==6&&id.get_name().substr(0,5)=="Drive"&&id.get_name().back()>='A'&&id.get_name().back()<='Z')&&!(id.get_name().size()>9&&id.get_name().substr(0,6)=="drive_"&&id.get_name()[6]>='B'&&id.get_name()[6]<='Z'&&id.get_name()[7]=='_')&&id.get_name()!="mapper_cycauto") {
            std::string text = id.get_text();
            if (id.get_name()=="hostkey_mapper"||id.get_name()=="clipboard_device") {
                std::size_t found = text.find(":");
                if (found!=std::string::npos) text = text.substr(0, found);
            }
            std::string idname = id.get_name().size()>9&&id.get_name().substr(0,8)=="drive_A_"?"drive_"+id.get_name().substr(8):id.get_name();
            if (!CodePageGuestToHostUTF8(temp,text.c_str()))
                fprintf(out,":MENU:%s\n%s\n.\n",idname.c_str(),text.c_str());
            else
                fprintf(out,":MENU:%s\n%s\n.\n",idname.c_str(),temp);
        }
	}
    std::map<std::string,std::string> event_map = get_event_map();
    for (auto it=event_map.begin();it!=event_map.end();++it) {
        if (mainMenu.item_exists("mapper_"+it->first) && mainMenu.get_item("mapper_"+it->first).get_text() == it->second) continue;
        if (!CodePageGuestToHostUTF8(temp,it->second.c_str()))
            fprintf(out,":MAPPER:%s\n%s\n.\n",it->first.c_str(),it->second.c_str());
        else
            fprintf(out,":MAPPER:%s\n%s\n.\n",it->first.c_str(),temp);
    }
	morelen=inmsg=false;
	fclose(out);
	return true;
}

void ResolvePath(std::string& in);
void MSG_Init() {
	Section_prop *section=static_cast<Section_prop *>(control->GetSection("dosbox"));

	if (control->opt_lang != "") {
		LoadMessageFile(control->opt_lang.c_str());
		SetVal("dosbox", "language", control->opt_lang.c_str());
        if (control->opt_langcp && msgcodepage>0 && isSupportedCP(msgcodepage)) {
            Section_prop *sec = static_cast<Section_prop *>(control->GetSection("config"));
            char cstr[20];
            cstr[0] = 0;
            if (sec!=NULL) {
                char *countrystr = (char *)sec->Get_string("country"), *r=strchr(countrystr, ',');
                if (r!=NULL) *r=0;
                if (strlen(countrystr)>10) countrystr[0] = 0;
                sprintf(cstr, "%s,%d", countrystr, msgcodepage);
                SetVal("config", "country", cstr);
                const char *imestr = section->Get_string("ime");
                if (tonoime && !strcasecmp(imestr, "auto") && (msgcodepage == 932 || msgcodepage == 936 || msgcodepage == 949 || msgcodepage == 950 || msgcodepage == 951)) {
                    tonoime = false;
                    enableime = true;
                    SetIME();
                }
            }
        }
        if (tonoime) {
            tonoime = enableime = false;
#if defined(WIN32) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
            ImmDisableIME((DWORD)(-1));
#endif
            SetIME();
        }
	}
	else {
		Prop_path* pathprop = section->Get_path("language");
        if (pathprop != NULL) {
            std::string path = pathprop->realpath;
            ResolvePath(path);
            if (testLoadLangFile(path.c_str()))
                LoadMessageFile(path.c_str());
            else {
                std::string lang = section->Get_string("language");
                if (lang.size()) LoadMessageFile(lang.c_str());
            }
        }
	}
    std::string showdbcsstr = static_cast<Section_prop *>(control->GetSection("dosv"))->Get_string("showdbcsnodosv");
#if defined(USE_TTF)
    showdbcs = showdbcsstr=="true"||showdbcsstr=="1"||(showdbcsstr=="auto" && (loadlang || dbcs_sbcs));
#else
    showdbcs = showdbcsstr=="true"||showdbcsstr=="1"||(showdbcsstr=="auto" && loadlang);
#endif
    if (!IS_EGAVGA_ARCH) showdbcs = false;
}
