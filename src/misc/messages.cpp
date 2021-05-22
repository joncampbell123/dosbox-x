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
#include "support.h"
#include "setup.h"
#include "control.h"
#include "menu.h"
#include <list>
#include <string>
using namespace std;

extern bool dos_kernel_disabled, force_conversion;
bool morelen = false, isSupportedCP(int newCP);
bool CodePageHostToGuestUTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/), CodePageGuestToHostUTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/);
std::string langname = "", langnote = "";
int msgcodepage = 0;
void menu_update_autocycle(void);

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
        Section_prop *section = static_cast<Section_prop *>(control->GetSection("config"));
        if (section!=NULL) {
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
        dos.loaded_codepage = IS_PC98_ARCH?932:437;
        return false;
    } else
        return true;
}

void LoadMessageFile(const char * fname) {
	if (!fname) return;
	if(*fname=='\0') return;//empty string=no languagefile

	LOG(LOG_MISC,LOG_DEBUG)("Loading message file %s",fname);

	FILE * mfile=fopen(fname,"rt");
	/* This should never happen and since other modules depend on this use a normal printf */
	if (!mfile) {
		std::string message="Could not load language message file '"+std::string(fname)+"'. The default language will be used.";
		bool systemmessagebox(char const * aTitle, char const * aMessage, char const * aDialogType, char const * aIconType, int aDefaultButton);
		systemmessagebox("Warning", message.c_str(), "ok","warning", 1);
		LOG_MSG("MSG:Cannot load language file: %s",fname);
		return;
	}
    msgcodepage = 0;
    langname = langnote = "";
	char linein[LINE_IN_MAXLEN];
	char name[LINE_IN_MAXLEN], menu_name[LINE_IN_MAXLEN];
	char string[LINE_IN_MAXLEN*10], temp[4096];
	/* Start out with empty strings */
	name[0]=0;string[0]=0;
    bool res=true;
    int cp=dos.loaded_codepage;
    if (!dos.loaded_codepage) res=InitCodePage();
	while(fgets(linein, LINE_IN_MAXLEN, mfile)!=0) {
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
                        if (!res && c>0 && isSupportedCP(c)) {
                            msgcodepage = c;
                            dos.loaded_codepage = c;
                        }
                    } else if (!strcmp(p, "LANGUAGE"))
                        langname = r+1;
                    else if (!strcmp(p, "REMARK"))
                        langnote = r+1;
                    *r=':';
                }
            } else if (!strncasecmp(linein+1, "MENU:", 5)) {
                *name=0;
                strcpy(menu_name,linein+6);
            } else {
                *menu_name=0;
                strcpy(name,linein+1);
            }
		/* End of string marker */
		} else if (linein[0]=='.') {
			/* Replace/Add the string to the internal languagefile */
			/* Remove last newline (marker is \n.\n) */
			size_t ll = strlen(string);
			if(ll && string[ll - 1] == '\n') string[ll - 1] = 0; //Second if should not be needed, but better be safe.
            if (strlen(name))
                MSG_Replace(name,string);
            else if (strlen(menu_name)&&mainMenu.item_exists(menu_name))
                mainMenu.get_item(menu_name).set_text(string);
		} else {
			/* Normal string to be added */
            if (!CodePageHostToGuestUTF8(temp,linein))
                strcat(string,linein);
            else
                strcat(string,temp);
			strcat(string,"\n");
		}
	}
	fclose(mfile);
    menu_update_autocycle();
    dos.loaded_codepage=cp;
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
	fprintf(out,":DOSBOX-X:VERSION:%s\n",VERSION);
	if (name!=NULL||langname!="") fprintf(out,":DOSBOX-X:LANGUAGE:%s\n",name!=NULL?name:langname.c_str());
	if (dos.loaded_codepage) fprintf(out,":DOSBOX-X:CODEPAGE:%d\n",dos.loaded_codepage);
	fprintf(out,":DOSBOX-X:REMARK:%s\n",langnote.c_str());
	morelen=true;
	for(itmb tel=Lang.begin();tel!=Lang.end();++tel){
        if (!CodePageGuestToHostUTF8(temp,(*tel).val.c_str()))
            fprintf(out,":%s\n%s\n.\n",(*tel).name.c_str(),(*tel).val.c_str());
        else
            fprintf(out,":%s\n%s\n.\n",(*tel).name.c_str(),temp);
	}
	std::vector<DOSBoxMenu::item> master_list = mainMenu.get_master_list();
	for (auto &id : master_list) {
		if (id.is_allocated()&&id.get_type()!=DOSBoxMenu::separator_type_id&&id.get_type()!=DOSBoxMenu::vseparator_type_id&&!(id.get_name().size()==5&&id.get_name().substr(0,4)=="slot")&&id.get_name()!="mapper_cycauto") {
            std::string text = id.get_text();
            if (id.get_name()=="hostkey_mapper"||id.get_name()=="clipboard_device") {
                std::size_t found = text.find(":");
                if (found!=std::string::npos) text = text.substr(0, found);
            }
            if (!CodePageGuestToHostUTF8(temp,text.c_str()))
                fprintf(out,":MENU:%s\n%s\n.\n",id.get_name().c_str(),text.c_str());
            else
                fprintf(out,":MENU:%s\n%s\n.\n",id.get_name().c_str(),temp);
        }
	}
	morelen=false;
	fclose(out);
	return true;
}

void ResolvePath(std::string& in);
void MSG_Init() {
	Section_prop *section=static_cast<Section_prop *>(control->GetSection("dosbox"));

	if (control->opt_lang != "") {
		LoadMessageFile(control->opt_lang.c_str());
	}
	else {
		Prop_path* pathprop = section->Get_path("language");
        if (pathprop != NULL) {
            std::string path = pathprop->realpath;
            ResolvePath(path);
            LoadMessageFile(path.c_str());
        }
	}
}
