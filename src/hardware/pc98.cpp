
#include "dosbox.h"
#include "setup.h"
#include "video.h"
#include "pic.h"
#include "vga.h"
#include "programs.h"
#include "support.h"
#include "setup.h"
#include "timer.h"
#include "mem.h"
#include "util_units.h"
#include "control.h"
#include "mixer.h"

#include <string.h>
#include <stdlib.h>
#include <string>
#include <stdio.h>

using namespace std;

extern bool gdc_5mhz_mode;
extern bool enable_pc98_egc;
extern bool enable_pc98_grcg;

void gdc_5mhz_mode_update_vars(void);
void gdc_egc_enable_update_vars(void);
void gdc_grcg_enable_update_vars(void);

/* ====================== PC98UTIL.COM ====================== */
class PC98UTIL : public Program {
public:
	void Run(void) {
        string arg;
		bool got_opt=false;
		
        cmd->BeginOpt();
        while (cmd->GetOpt(/*&*/arg)) {
			got_opt=true;
            if (arg == "?" || arg == "help") {
                doHelp();
                break;
            }
            else if (arg == "egc") {
                enable_pc98_egc = true;
                WriteOut("EGC graphics functions enabled\n");
                gdc_egc_enable_update_vars();
				if (!enable_pc98_grcg) { //Enable GRCG if not enabled
					enable_pc98_grcg = true;
					gdc_grcg_enable_update_vars();
				}
#if defined(WIN32) && !defined(C_SDL2)
				int Reflect_Menu(void);
				Reflect_Menu();
#endif
            }
            else if (arg == "noegc") {
                enable_pc98_egc = false;
                WriteOut("EGC graphics functions disabled\n");
                gdc_egc_enable_update_vars();
#if defined(WIN32) && !defined(C_SDL2)
				int Reflect_Menu(void);
				Reflect_Menu();
#endif
            }
            else if (arg == "gdc25") {
                gdc_5mhz_mode = false;
                gdc_5mhz_mode_update_vars();
                LOG_MSG("PC-98: GDC is running at %.1fMHz.",gdc_5mhz_mode ? 5.0 : 2.5);
                WriteOut("GDC is now running at 2.5MHz\n");
#if defined(WIN32) && !defined(C_SDL2)
				int Reflect_Menu(void);
				Reflect_Menu();
#endif
            }
            else if (arg == "gdc50") {
                gdc_5mhz_mode = true;
                gdc_5mhz_mode_update_vars();
                LOG_MSG("PC-98: GDC is running at %.1fMHz.",gdc_5mhz_mode ? 5.0 : 2.5);
                WriteOut("GDC is now running at 5MHz\n");
#if defined(WIN32) && !defined(C_SDL2)
				int Reflect_Menu(void);
				Reflect_Menu();
#endif
            }
            else {
                WriteOut("Unknown switch %s",arg.c_str());
                break;
            }
        }
        cmd->EndOpt();
		if(!got_opt) doHelp();
	}
    void doHelp(void) {
        WriteOut("PC98UTIL PC-98 emulation utility\n");
        WriteOut("  /gdc25     Set GDC to 2.5MHz\n");
        WriteOut("  /gdc50     Set GDC to 5.0MHz\n");
        WriteOut("  /egc       Enable EGC\n");
        WriteOut("  /noegc     Disable EGC\n");
    }
};

void PC98UTIL_ProgramStart(Program * * make) {
	*make=new PC98UTIL;
}
/*==============================================*/

/* wait-delay I/O port of some kind */
void pc98_wait_write(Bitu port,Bitu val,Bitu iolen) {
    unsigned int wait_cycles = (unsigned int)(CPU_CycleMax * 0.0006); /* 0.6us = 0.0006ms */

    CPU_Cycles -= wait_cycles;
}

