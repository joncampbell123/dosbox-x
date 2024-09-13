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


#ifndef DOSBOX_CONTROL_H
#define DOSBOX_CONTROL_H

#ifdef _MSC_VER
//#pragma warning ( disable : 4786 )
//#pragma warning ( disable : 4290 )
#endif

#include "programs.h"
#include "setup.h"

class Config{
public:
    CommandLine * cmdline;
private:
    std::list<Section*> sectionlist;
    typedef std::list<Section*>::iterator it;
    typedef std::list<Section*>::reverse_iterator reverse_it;
    typedef std::list<Section*>::const_iterator const_it;
    typedef std::list<Section*>::const_reverse_iterator const_reverse_it;
//  void (* _start_function)(void);
    bool secure_mode = false; //Sandbox mode
public:
    bool initialised = false;
    std::vector<std::string> auto_bat_additional;
    std::vector<std::string> startup_params;
    std::vector<std::string> configfiles;
    Config(CommandLine * cmd):cmdline(cmd) {
        startup_params.emplace_back(cmdline->GetFileName());
        cmdline->FillVector(startup_params);
    }
    ~Config();

    Section_line * AddSection_line(char const * const _name,void (*_initfunction)(Section*));
    Section_prop * AddSection_prop(char const * const _name,void (*_initfunction)(Section*),bool canchange=false);
    
    Section* GetSection(int index);
    Section* GetSection(std::string const&_sectionname) const;
    Section* GetSectionFromProperty(char const * const prop) const;

    bool PrintConfig(char const * const configfilename,int everything=-1,bool norem=false) const;
    bool ParseConfigFile(char const * const configfilename);
    void ParseEnv(char ** envp);
    bool SecureMode() const { return secure_mode; }
    void SwitchToSecureMode() { secure_mode = true; }//can't be undone
    void ClearExtraData() { Section_prop *sec_prop; Section_line *sec_line; for (const_it tel = sectionlist.begin(); tel != sectionlist.end(); ++tel) {sec_prop = dynamic_cast<Section_prop *>(*tel); sec_line = dynamic_cast<Section_line *>(*tel); if (sec_prop) sec_prop->data = ""; else if (sec_line) sec_line->data = "";} }
public:
    std::string opt_editconf,opt_opensaves,opt_opencaptures,opt_lang="",opt_machine="";
    std::vector<std::string> config_file_list;
    std::vector<std::string> opt_o;
    std::vector<std::string> opt_c;
    std::vector<std::string> opt_set;

    double opt_time_limit = -1;
    signed char opt_promptfolder = -1;
    bool opt_disable_dpi_awareness = false;
    bool opt_disable_numlock_check = false;
    bool opt_date_host_forced = false;
    bool opt_used_defaultdir = false;
    bool opt_defaultmapper = false;
    bool opt_fastbioslogo = false;
    bool opt_print_ticks = false;
    bool opt_break_start = false;
    bool opt_erasemapper = false;
    bool opt_resetmapper = false;
    bool opt_startmapper = false;
    bool opt_defaultconf = false;
    bool opt_noautoexec = false;
    bool opt_securemode = false;
    bool opt_fullscreen = false;
    bool opt_fastlaunch = false;
    bool opt_showcycles = false;
    bool opt_earlydebug = false;
    bool opt_logfileio = false;
    bool opt_noconsole = false;
    bool opt_eraseconf = false;
    bool opt_resetconf = false;
    bool opt_printconf = false;
    bool opt_logint21 = false;
    bool opt_display2 = false;
    bool opt_userconf = false;
    bool opt_noconfig = false;
    bool opt_log_con = false;
    bool opt_console = false;
    bool opt_startui = false;
    bool opt_silent = false;
    bool opt_showrt = false;
    bool opt_nomenu = false;
    bool opt_prerun = false;
    bool opt_langcp = false;  // True if command line option -langcp is specified, always use codepage specified by the language file
    bool opt_debug = false;
    bool opt_nogui = false;
    bool opt_nolog = false;
    bool opt_exit = false;
    bool opt_test = false;
};

#endif
