/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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


#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctime>
#include <unistd.h>
#include "dosbox.h"
#include "debug.h"
#include "cpu.h"
#include "video.h"
#include "pic.h"
#include "cpu.h"
#include "callback.h"
#include "inout.h"
#include "mixer.h"
#include "timer.h"
#include "dos_inc.h"
#include "setup.h"
#include "control.h"
#include "cross.h"
#include "programs.h"
#include "support.h"
#include "mapper.h"
#include "ints/int10.h"
#include "menu.h"
#include "render.h"
#include "pci_bus.h"
#include "parport.h"
#include "clockdomain.h"

#include <list>

extern ClockDomain		clockdom_8254_PIT;

/* ISA bus OSC clock (14.31818MHz) */
/*  +---- / 12 = PIT timer clock 1.1931816666... MHz */
ClockDomain		clockdom_ISA_OSC(14318180);		/* MASTER 14.31818MHz */
ClockDomain		clockdom_8254_PIT(14318180,12);		/* SLAVE  14.31818MHz / 12 = 1.1931816666666.... MHz */

/* ISA bus BCLK clock (8.3333MHz) */
ClockDomain		clockdom_ISA_BCLK(25000000,3);		/* MASTER 25000000Hz / 3 = 8.333333MHz */

/* Clocks to be updated by Normal loop */
std::list<ClockDomain*>	clockdom_top_update;			/* list of master clocks at the top of the clock tree */

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if C_NE2000
//#include "ne2000.h"
void NE2K_Init(Section* sec);
#endif

Config * control;
MachineType machine;
bool PS1AudioCard;		// Perhaps have PS1 as a machine type...?
SVGACards svgaCard;

/* The whole load of startups for all the subfunctions */
void MSG_Init(Section_prop *);
void LOG_StartUp(void);
void MEM_Init(Section *);
void PAGING_Init(Section *);
void ISAPNP_Cfg_Init(Section *);
void IO_Init(Section * );
void CALLBACK_Init(Section*);
void PROGRAMS_Init(Section*);
//void CREDITS_Init(Section*);
void RENDER_Init(Section*);
void VGA_VsyncInit(Section*);
void VGA_Init(Section*);

void DOS_Init(Section*);


void CPU_Init(Section*);

#if C_FPU
void FPU_Init(Section*);
#endif

void DMA_Init(Section*);

void MIXER_Init(Section*);
void MIDI_Init(Section*);
void HARDWARE_Init(Section*);

#if defined(PCI_FUNCTIONALITY_ENABLED)
void PCI_Init(Section*);
void VOODOO_Init(Section*);
#endif


void IDE_Primary_Init(Section*);
void IDE_Secondary_Init(Section*);
void IDE_Tertiary_Init(Section*);
void IDE_Quaternary_Init(Section*);

void KEYBOARD_Init(Section*);	//TODO This should setup INT 16 too but ok ;)
void JOYSTICK_Init(Section*);
void MOUSE_Init(Section*);
void SBLASTER_Init(Section*);
void GUS_Init(Section*);
void MPU401_Init(Section*);
void PCSPEAKER_Init(Section*);
void TANDYSOUND_Init(Section*);
void DISNEY_Init(Section*);
void PS1SOUND_Init(Section*);
void INNOVA_Init(Section*);
void SERIAL_Init(Section*); 
void DONGLE_Init(Section*);

#if C_IPX
void IPX_Init(Section*);
#endif

void SID_Init(Section* sec);

void PIC_Init(Section*);
void TIMER_Init(Section*);
void BIOS_Init(Section*);
void DEBUG_Init(Section*);
void CMOS_Init(Section*);

void MSCDEX_Init(Section*);
void DRIVES_Init(Section*);
void CDROM_Image_Init(Section*);

/* Dos Internal mostly */
void EMS_Init(Section*);
void XMS_Init(Section*);

void DOS_KeyboardLayout_Init(Section*);

void AUTOEXEC_Init(Section*);
void SHELL_Init(void);

void INT10_Init(Section*);

static LoopHandler * loop;

bool SDLNetInited;

#ifdef __WIN32__
void MSG_Loop(void);
#endif

static Bit32u ticksRemain;
static Bit32u ticksLast;
static Bit32u ticksAdded;
Bit32s ticksDone;
Bit32u ticksScheduled;
bool ticksLocked;
bool mono_cga=false;

static Bit32u Ticks = 0;

extern void GFX_SetTitle(Bit32s cycles,Bits frameskip,Bits timing,bool paused);
extern Bitu cycle_count;
extern Bitu frames;

#ifdef __SSE__
bool sse1_available = false;
bool sse2_available = false;

#if !defined (__APPLE__)
#ifdef __GNUC__
#define cpuid(func,ax,bx,cx,dx)\
	__asm__ __volatile__ ("cpuid":\
	"=a" (ax), "=b" (bx), "=c" (cx), "=d" (dx) : "a" (func));
#endif
#if defined(_MSC_VER)
#define cpuid(func,a,b,c,d)\
    __asm mov eax, func\
    __asm cpuid\
    __asm mov a, eax\
    __asm mov b, ebx\
    __asm mov c, ecx\
    __asm mov d, edx
#endif
#endif

class CLOCKDOM : public Program {
public:
	void Run(void) {
		for (std::list<ClockDomain*>::iterator i=clockdom_top_update.begin();i!=clockdom_top_update.end();i++)
			(*i)->snapshot();

		WriteOut("%zu clock domains\n",clockdom_top_update.size());
		for (std::list<ClockDomain*>::iterator i=clockdom_top_update.begin();i!=clockdom_top_update.end();i++) {
			WriteOut(" %c  %-15s count=%llu freq=%.1fHz (%llu/%llu)\n",
				(*i)->master ? 'M' : 'S',
				(*i)->name.c_str(),
				(*i)->counter_whole_snapshot,
				((double)(*i)->freq) / (*i)->freq_div,
				(*i)->freq,(*i)->freq_div);

			for (std::vector<ClockDomain*>::iterator j=(*i)->slaves.begin();j!=(*i)->slaves.end();j++) {
				WriteOut(" +%c %-15s count=%llu freq=%.1fHz (%llu/%llu)\n",
					(*j)->master ? 'M' : 'S',
					(*j)->name.c_str(),
					(*j)->counter_whole_snapshot,
					((double)(*j)->freq) / (*j)->freq_div,
					(*j)->freq,(*j)->freq_div);
			}
		}
	}
};

void CLOCKDOM_ProgramStart(Program * * make) {
	*make=new CLOCKDOM;
}

void CheckSSESupport()
{
#if defined (__APPLE__)
#elif defined (__GNUC__) || (_MSC_VER)
	Bitu a, b, c, d;
	cpuid(1, a, b, c, d);
	if((d >> 26) & 1) {
		sse1_available = true;
		sse2_available = true;
		//LOG_MSG("SSE2 available");
	} else if((d >> 25) & 1) {
		sse1_available = true;
		sse2_available = false;
		//LOG_MSG("SSE1 available");
	}
#else
	//LOG_MSG("Can't check if SSE is available... sorry.");
#endif
}
#endif

void run_hw() {
	double f = PIC_FullIndex() / 1000,next_f,p_next_f=0;
	unsigned long long t_next;
	unsigned long long t_clk;
	int patience = 1000;
	bool again;

	do {
		again=false;
		next_f=f;

		for (std::list<ClockDomain*>::iterator i=clockdom_top_update.begin();i!=clockdom_top_update.end();i++) {
			t_clk = (*i)->time_to_clocks(next_f);
			if ((*i)->next_event_time(/*&*/t_next) && t_clk > t_next)
				next_f = (*i)->clocks_to_time(t_next);
		}

		/* in case floating point errors cause next_f to NOT QUITE REACH the next event, be prepared to give it a nudge */
		if (next_f == p_next_f) next_f += (2.0 * clockdom_ISA_BCLK.freq_div) / clockdom_ISA_BCLK.freq;

		for (std::list<ClockDomain*>::iterator i=clockdom_top_update.begin();i!=clockdom_top_update.end();i++) {
			t_clk = (*i)->time_to_clocks(next_f);
			if (t_clk > (*i)->counter) (*i)->advance(t_clk - (*i)->counter);
			(*i)->fire_events();
		}

		p_next_f = next_f;
	} while (again && --patience > 0);

	if (patience <= 0)
		fprintf(stderr,"WARNING: run_hw() one or more clock events are stuck?\n");
}

static Bitu Normal_Loop(void) {
	Bits ret;
	while (1) {
		run_hw();
		if (PIC_RunQueue()) {
			Bit32u ticksNew;
			ticksNew=GetTicks();
		    if(ticksNew>=Ticks) {
				CPU_CyclesCur=(cycle_count-CPU_CyclesCur) >> 9;
				Ticks=ticksNew + 512;		// next update in 512ms
				frames*=1.953;			// compensate for 512ms interval
				if(!menu.hidecycles) GFX_SetTitle(CPU_CycleMax,-1,-1,false);
				CPU_CyclesCur=cycle_count;
				frames=0;
		    }
			ret = (*cpudecoder)();
			if (GCC_UNLIKELY(ret<0)) return 1;
			if (ret>0) {
				if (GCC_UNLIKELY(ret >= CB_MAX)) return 0;
				Bitu blah = (*CallBack_Handlers[ret])();
				if (GCC_UNLIKELY(blah)) return blah;
			}
#if C_DEBUG
			if (DEBUG_ExitLoop()) return 0;
#endif
		} else {
#ifdef __WIN32__
			MSG_Loop();
#endif
			GFX_Events();
			extern bool DOSBox_Paused();
			if (DOSBox_Paused() == false && ticksRemain>0) {
				TIMER_AddTick();
				ticksRemain--;
			} else goto increaseticks;
		}
	}
increaseticks:
	if (GCC_UNLIKELY(ticksLocked)) {
		ticksRemain=5;
		/* Reset any auto cycle guessing for this frame */
		ticksLast = GetTicks();
		ticksAdded = 0;
		ticksDone = 0;
		ticksScheduled = 0;
	} else {
		Bit32u ticksNew;
		ticksNew=GetTicks();
		ticksScheduled += ticksAdded;
		if (ticksNew > ticksLast) {
			ticksRemain = ticksNew-ticksLast;
			ticksLast = ticksNew;
			ticksDone += ticksRemain;
			if ( ticksRemain > 20 ) {
				ticksRemain = 20;
			}
			ticksAdded = ticksRemain;
			if (CPU_CycleAutoAdjust && !CPU_SkipCycleAutoAdjust) {
				if (ticksScheduled >= 250 || ticksDone >= 250 || (ticksAdded > 15 && ticksScheduled >= 5) ) {
					if(ticksDone < 1) ticksDone = 1; // Protect against div by zero
					/* ratio we are aiming for is around 90% usage*/
					Bit32s ratio = (ticksScheduled * (CPU_CyclePercUsed*90*1024/100/100)) / ticksDone;
					Bit32s new_cmax = CPU_CycleMax;
					Bit64s cproc = (Bit64s)CPU_CycleMax * (Bit64s)ticksScheduled;
					if (cproc > 0) {
						/* ignore the cycles added due to the IO delay code in order
						   to have smoother auto cycle adjustments */
						double ratioremoved = (double) CPU_IODelayRemoved / (double) cproc;
						if (ratioremoved < 1.0) {
							ratio = (Bit32s)((double)ratio * (1 - ratioremoved));
							/* Don't allow very high ratio which can cause us to lock as we don't scale down
							 * for very low ratios. High ratio might result because of timing resolution */
							if (ticksScheduled >= 250 && ticksDone < 10 && ratio > 20480) 
								ratio = 20480;
							Bit64s cmax_scaled = (Bit64s)CPU_CycleMax * (Bit64s)ratio;
							/* The auto cycle code seems reliable enough to disable the fast cut back code.
							 * This should improve the fluency of complex games.
							if (ratio <= 1024) 
								new_cmax = (Bit32s)(cmax_scaled / (Bit64s)1024);
							else 
							 */
							new_cmax = (Bit32s)(1 + (CPU_CycleMax >> 1) + cmax_scaled / (Bit64s)2048);
						}
					}

					if (new_cmax<CPU_CYCLES_LOWER_LIMIT)
						new_cmax=CPU_CYCLES_LOWER_LIMIT;

					/* ratios below 1% are considered to be dropouts due to
					   temporary load imbalance, the cycles adjusting is skipped */
					if (ratio>10) {
						/* ratios below 12% along with a large time since the last update
						   has taken place are most likely caused by heavy load through a
						   different application, the cycles adjusting is skipped as well */
						if ((ratio>120) || (ticksDone<700)) {
							CPU_CycleMax = new_cmax;
							if (CPU_CycleLimit > 0) {
								if (CPU_CycleMax>CPU_CycleLimit) CPU_CycleMax = CPU_CycleLimit;
							}
						}
					}
					CPU_IODelayRemoved = 0;
					ticksDone = 0;
					ticksScheduled = 0;
				} else if (ticksAdded > 15) {
					/* ticksAdded > 15 but ticksScheduled < 5, lower the cycles
					   but do not reset the scheduled/done ticks to take them into
					   account during the next auto cycle adjustment */
					CPU_CycleMax /= 3;
					if (CPU_CycleMax < CPU_CYCLES_LOWER_LIMIT)
						CPU_CycleMax = CPU_CYCLES_LOWER_LIMIT;
				}
			}
		} else {
			ticksAdded = 0;
			SDL_Delay(1);
			ticksDone -= GetTicks() - ticksNew;
			if (ticksDone < 0)
				ticksDone = 0;
		}
	}
	return 0;
}

void DOSBOX_SetLoop(LoopHandler * handler) {
	loop=handler;
}

void DOSBOX_SetNormalLoop() {
	loop=Normal_Loop;
}

void DOSBOX_RunMachine(void){
	Bitu ret;
	do {
		ret=(*loop)();
	} while (!ret);
}

static void DOSBOX_UnlockSpeed( bool pressed ) {
	static bool autoadjust = false;
	if (pressed) {
		LOG_MSG("Fast Forward ON");
		ticksLocked = true;
		if (CPU_CycleAutoAdjust) {
			autoadjust = true;
			CPU_CycleAutoAdjust = false;
			CPU_CycleMax /= 3;
			if (CPU_CycleMax<1000) CPU_CycleMax=1000;
		}
	} else {
		LOG_MSG("Fast Forward OFF");
		ticksLocked = false;
		if (autoadjust) {
			autoadjust = false;
			CPU_CycleAutoAdjust = true;
		}
	}
	GFX_SetTitle(-1,-1,-1,false);
}

void DOSBOX_UnlockSpeed2( bool pressed ) {
	if (pressed) {
		ticksLocked =! ticksLocked;
		DOSBOX_UnlockSpeed(ticksLocked?true:false);
	}
}

namespace
{
std::string getTime()
{
    const time_t current = time(NULL);
    tm* timeinfo;
    timeinfo = localtime(&current); //convert to local time
    char buffer[50];
    ::strftime(buffer, 50, "%H:%M:%S", timeinfo);
    return buffer;
}

void notifyError(const std::string& message)
{
#ifdef WIN32
    ::MessageBox(0, message.c_str(), "Error", 0);
#endif
    LOG_MSG(message.c_str());
}

void SetGameState(int value) {
}

void SaveGameState(bool pressed) {
}

void LoadGameState(bool pressed) {
}

void NextSaveSlot(bool pressed) {
}

void PreviousSaveSlot(bool pressed) {
}
}
void SetGameState_Run(int value) { SetGameState(value); }
void SaveGameState_Run(void) { SaveGameState(true); }
void LoadGameState_Run(void) { LoadGameState(true); }
void NextSaveSlot_Run(void) { NextSaveSlot(true); }
void PreviousSaveSlot_Run(void) { PreviousSaveSlot(true); }

bool ignore_opcode_63 = true;
bool mainline_compatible_mapping = true;
Bitu VGA_BIOS_Size_override = 0;

extern bool dynamic_dos_kernel_alloc;
extern Bitu DOS_PRIVATE_SEGMENT_Size;

/* tied to ISA BCLK clock. test callback */
void ISA_BCLK_test_event(ClockDomainEvent *e) {
	fprintf(stderr,"%s test, counter=%llu %lld clocks*div late\n",
		e->domain->name.c_str(),e->domain->counter,
		(signed long long)(e->domain->counter - e->t_clock));

	e->domain->add_event_rel(ISA_BCLK_test_event,e->domain->freq);
}

static void DOSBOX_RealInit(Section * sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	/* Initialize some dosbox internals */

	ticksRemain=0;
	ticksLast=GetTicks();
	ticksLocked = false;
	DOSBOX_SetLoop(&Normal_Loop);
	MSG_Init(section);

	mainline_compatible_mapping = section->Get_bool("mainline compatible mapping");
	VGA_BIOS_Size_override = section->Get_int("vga bios size override");
	if (VGA_BIOS_Size_override > 0) VGA_BIOS_Size_override = (VGA_BIOS_Size_override+0x7FF)&(~0xFFF);

	DOS_PRIVATE_SEGMENT_Size = section->Get_int("private area size");
	DOS_PRIVATE_SEGMENT_Size += 8;
	DOS_PRIVATE_SEGMENT_Size >>= 4;

	MAPPER_AddHandler(DOSBOX_UnlockSpeed, MK_f12, MMOD2,"speedlock","Speedlock");
	MAPPER_AddHandler(DOSBOX_UnlockSpeed2, MK_f11, MMOD2,"speedlock2","Speedlock2");
	std::string cmd_machine;
	if (control->cmdline->FindString("-machine",cmd_machine,true)){
		//update value in config (else no matching against suggested values
		section->HandleInputline(std::string("machine=") + cmd_machine);
	}

//#pragma message("WARNING: adapt keys!!!!")

    //add support for loading/saving game states
    MAPPER_AddHandler(SaveGameState, MK_f5, MMOD2,"savestate","Save State");
    MAPPER_AddHandler(LoadGameState, MK_f9, MMOD2,"loadstate","Load State");
    MAPPER_AddHandler(PreviousSaveSlot, MK_f6, MMOD2,"prevslot","Prev. Slot");
    MAPPER_AddHandler(NextSaveSlot, MK_f7, MMOD2,"nextslot","Next Slot");

	std::string mtype(section->Get_string("machine"));
	svgaCard = SVGA_None; 
	machine = MCH_VGA;
	int10.vesa_nolfb = false;
	int10.vesa_oldvbe = false;
	if      (mtype == "cga")      { machine = MCH_CGA; mono_cga = false; }
	else if (mtype == "cga_mono") { machine = MCH_CGA; mono_cga = true; }
	else if (mtype == "tandy")    { machine = MCH_TANDY; }
	else if (mtype == "pcjr")     { machine = MCH_PCJR; }
	else if (mtype == "hercules") { machine = MCH_HERC; }
	else if (mtype == "ega")      { machine = MCH_EGA; }
//	else if (mtype == "vga")          { svgaCard = SVGA_S3Trio; }
	else if (mtype == "svga_s3")       { svgaCard = SVGA_S3Trio; }
	else if (mtype == "vesa_nolfb")   { svgaCard = SVGA_S3Trio; int10.vesa_nolfb = true;}
	else if (mtype == "vesa_oldvbe")   { svgaCard = SVGA_S3Trio; int10.vesa_oldvbe = true;}
	else if (mtype == "svga_et4000")   { svgaCard = SVGA_TsengET4K; }
	else if (mtype == "svga_et3000")   { svgaCard = SVGA_TsengET3K; }
//	else if (mtype == "vga_pvga1a")   { svgaCard = SVGA_ParadisePVGA1A; }
	else if (mtype == "svga_paradise") { svgaCard = SVGA_ParadisePVGA1A; }
	else if (mtype == "vgaonly")      { svgaCard = SVGA_None; }
	else if (mtype == "amstrad")      { machine = MCH_AMSTRAD; }
	else E_Exit("DOSBOX:Unknown machine type %s",mtype.c_str());
	// Hack!
	//mtype=MCH_AMSTRAD;

	std::string isabclk = section->Get_string("isa bus clock");
	if (isabclk == "std8.3")
		clockdom_ISA_BCLK.set_frequency(25000000,3);	/* 25MHz / 3 = 8.333MHz, early 386 systems did this, became an industry standard "norm" afterwards */
	else if (isabclk == "std8")
		clockdom_ISA_BCLK.set_frequency(8000000,1);	/* 8Mhz */
	else if (isabclk == "std6")
		clockdom_ISA_BCLK.set_frequency(6000000,1);	/* 6MHz */
	else if (isabclk == "std4.77")
		clockdom_ISA_BCLK.set_frequency(clockdom_ISA_OSC.freq,3); /* 14.31818MHz / 3 = 4.77MHz */
	else if (isabclk == "oc10")
		clockdom_ISA_BCLK.set_frequency(10000000,1);	/* 10MHz */
	else if (isabclk == "oc12")
		clockdom_ISA_BCLK.set_frequency(12000000,1);	/* 12MHz */
	else if (isabclk == "oc15")
		clockdom_ISA_BCLK.set_frequency(15000000,1);	/* 15MHz */
	else if (isabclk == "oc16")
		clockdom_ISA_BCLK.set_frequency(16000000,1);	/* 16MHz */
	else {
		const char *s = isabclk.c_str(),*d;

		/* we're expecting an integer, a float, or an integer ratio */
		d = strchr(s,'/');
		if (d != NULL) { /* it has a slash therefore an integer ratio */
			unsigned long long num,den;

			while (*d == ' ' || *d == '/') d++;
			num = strtoull(s,NULL,0);
			den = strtoull(d,NULL,0);
			if (num >= 1ULL && den >= 1ULL) clockdom_ISA_BCLK.set_frequency(num,den);
		}
		else {
			d = strchr(s,'.');
			if (d != NULL) { /* it has a dot, floating point */
				double f = atof(s);
				unsigned long long fi = (unsigned long long)floor((f*1000000)+0.5);
				unsigned long long den = 1000000;

				while (den > 1ULL) {
					if ((fi%10ULL) == 0) {
						den /= 10ULL;
						fi /= 10ULL;
					}
					else {
						break;
					}
				}

				if (fi >= 1ULL) clockdom_ISA_BCLK.set_frequency(fi,den);
			}
			else {
				unsigned long long f = strtoull(s,NULL,10);
				if (f >= 1ULL) clockdom_ISA_BCLK.set_frequency(f,1);
			}
		}
	}

	/* MASTER: ISA OSC (14.31818MHz) */
	clockdom_ISA_OSC.set_name("ISA OSC");
	clockdom_top_update.push_back(&clockdom_ISA_OSC);
	/* SLAVE: PIT clock */
	clockdom_8254_PIT.set_name("8254 PIT");
	clockdom_ISA_OSC.add_slave(&clockdom_8254_PIT);

	/* MASTER: ISA BCLK (bus clock) (8.333333MHz) */
	clockdom_ISA_BCLK.set_name("ISA BCLK");
	clockdom_top_update.push_back(&clockdom_ISA_BCLK);

	/* MASTER event dispatch testing */
	clockdom_ISA_BCLK.add_event_rel(ISA_BCLK_test_event,clockdom_ISA_BCLK.freq);
	clockdom_ISA_OSC.add_event_rel(ISA_BCLK_test_event,clockdom_ISA_OSC.freq);
	/* SLAVE event dispatch testing */
	clockdom_8254_PIT.add_event_rel(ISA_BCLK_test_event,clockdom_8254_PIT.freq);
}


void DOSBOX_Init(void) {

#ifdef __SSE__
	CheckSSESupport();
#endif

	Section_prop * secprop;
	Section_line * secline;
	Prop_int* Pint;
	Prop_hex* Phex;
	Prop_string* Pstring;
	Prop_bool* Pbool;
	Prop_multival* Pmulti;
	Prop_multival_remain* Pmulti_remain;

	SDLNetInited = false;

	// Some frequently used option sets
	const char *rates[] = {  "44100", "48000", "32000","22050", "16000", "11025", "8000", "49716", 0 };
	const char *oplrates[] = {   "44100", "49716", "48000", "32000","22050", "16000", "11025", "8000", 0 };
	const char *ios[] = { "220", "240", "260", "280", "2a0", "2c0", "2e0", "300", 0 };
	const char *irqssb[] = { "7", "5", "3", "9", "10", "11", "12", 0 };
	const char *dmassb[] = { "1", "5", "0", "3", "6", "7", 0 };
	const char *iosgus[] = { "240", "220", "260", "280", "2a0", "2c0", "2e0", "300", 0 };
	const char *irqsgus[] = { "5", "3", "7", "9", "10", "11", "12", 0 };
	const char *dmasgus[] = { "3", "0", "1", "5", "6", "7", 0 };


	/* Setup all the different modules making up DOSBox */
	const char* machines[] = {
		"hercules", "cga", "cga_mono", "tandy", "pcjr", "ega",
		"vgaonly", "svga_s3", "svga_et3000", "svga_et4000",
		"svga_paradise", "vesa_nolfb", "vesa_oldvbe", "amstrad", 0 };
	secprop=control->AddSection_prop("dosbox",&DOSBOX_RealInit);
	Pstring = secprop->Add_path("language",Property::Changeable::Always,"");
	Pstring->Set_help("Select another language file.");

	Pstring = secprop->Add_string("machine",Property::Changeable::OnlyAtStart,"svga_s3");
	Pstring->Set_values(machines);
	Pstring->Set_help("The type of machine DOSBox tries to emulate.");

	Pint = secprop->Add_int("vmemsize", Property::Changeable::WhenIdle,2);
	Pint->SetMinMax(0,8);
	Pint->Set_help(
		"Amount of video memory in megabytes.\n"
		"  The maximum resolution and color depth the svga_s3 will be able to display\n"
		"  is determined by this value.\n "
		"  0: 512k (800x600  at 256 colors)\n"
		"  1: 1024x768  at 256 colors or 800x600  at 64k colors\n"
		"  2: 1600x1200 at 256 colors or 1024x768 at 64k colors or 640x480 at 16M colors\n"
		"  4: 1600x1200 at 64k colors or 1024x768 at 16M colors\n"
		"  8: up to 1600x1200 at 16M colors\n"
		"For build engine games, use more memory than in the list above so it can\n"
		"use triple buffering and thus won't flicker.\n"
		);

	Pstring = secprop->Add_path("captures",Property::Changeable::Always,"capture");
	Pstring->Set_help("Directory where things like wave, midi, screenshot get captured.");

	Pbool = secprop->Add_bool("mainline compatible mapping",Property::Changeable::OnlyAtStart,true);
	Pbool->Set_help("If set, arrange private areas, UMBs, and DOS kernel structures by default in the same way the mainline branch would do it.\n"
			"If cleared, these areas are allocated dynamically which may improve available memory and emulation accuracy.\n"
			"If your DOS game breaks under DOSBox-X but works with mainline DOSBox setting this option may help.");

	Pbool = secprop->Add_bool("adapter rom is ram",Property::Changeable::OnlyAtStart,false);
	Pbool->Set_help("Map adapter ROM as RAM (mainline DOSBox 0.74 behavior). When clear, unused adapter ROM is mapped out");

	Pint = secprop->Add_int("private area size",Property::Changeable::OnlyAtStart,32768); // DOSBox mainline compatible 32KB region
	Pint->SetMinMax(16,128*1024);
	Pint->Set_help("Set DOSBox-X private memory area size. This area contains private memory structures used by the DOS kernel.\n"
			"It is discarded when you boot into another OS. Mainline DOSBox uses 32KB. Testing shows that it is possible\n"
			"to run DOSBox with as little as 4KB. If DOSBox-X aborts with error \"not enough memory for internal tables\"\n"
			"then you need to increase this value.");

	Pstring = secprop->Add_string("a20",Property::Changeable::WhenIdle,"fast");
	Pstring->Set_help("A20 gate emulation mode.\n"
			  "The on/off/on_fake/off_fake options are intended for testing and debugging DOS development,\n"
			  "or to emulate obscure hardware, or to work around potential extended memory problems with DOS programs.\n"
			  "on_fake/off_fake are intended to test whether a program carries out a memory test to ensure the A20\n"
			  "gate is set as intended (as HIMEM.SYS does). If it goes by the gate bit alone, it WILL crash.\n"
			  "This parameter is also changeable from the builtin A20GATE command.\n"
			  "  fast                         Emulate A20 gating by remapping the first 64KB @ 1MB boundary (fast, mainline DOSBox behavior)\n"
			  "  mask                         Emulate A20 gating by masking memory I/O address (accurate)\n"
			  "  off                          Lock A20 gate off (Software/OS cannot enable A20)\n"
			  "  on                           Lock A20 gate on (Software/OS cannot disable A20)\n"
			  "  off_fake                     Lock A20 gate off but allow bit to toggle (hope your DOS game tests the HMA!)\n"
			  "  on_fake                      Lock A20 gate on but allow bit to toggle");

	Pstring = secprop->Add_string("isa bus clock",Property::Changeable::WhenIdle,"std8.3");
	Pstring->Set_help("ISA BCLK frequency.\n"
			  "WARNING: In future revisions, PCI/motherboard chipset emulation will allow the guest OS/program to alter this value at runtime.\n"
			  "  std8.3                       8.333MHz (typical 386-class or higher)\n"
			  "  std8                         8MHz\n"
			  "  std6                         6MHz\n"
			  "  std4.77                      4.77MHz (precisely 1/3 x 14.31818MHz). Bus frequency of older PC/XT systems.\n"
			  "  oc10                         10MHz\n"
			  "  oc12                         12MHz\n"
			  "  oc15                         15MHz\n"
			  "  oc16                         16MHz\n"
			  "  <integer or float>           Any integer or floating point value will be used as the clock frequency in Hz\n"
			  "  <integer/integer ratio>      If a ratio is given (num/den), the ratio will be used as the clock frequency");

#if C_DEBUG	
	LOG_StartUp();
#endif
	
	secprop->AddInitFunction(&IO_Init);//done
	secprop->AddInitFunction(&PAGING_Init);//done
	secprop->AddInitFunction(&MEM_Init);//done
	secprop->AddInitFunction(&HARDWARE_Init);//done
	Pint = secprop->Add_int("memsize", Property::Changeable::WhenIdle,16);
	Pint->SetMinMax(1,511);
	Pint->Set_help(
		"Amount of memory DOSBox has in megabytes.\n"
		"  This value is best left at its default to avoid problems with some games,\n"
		"  though few games might require a higher value.\n"
		"  There is generally no speed advantage when raising this value.");

	Pint = secprop->Add_int("memsizekb", Property::Changeable::WhenIdle,0);
	Pint->SetMinMax(0,524288);
	Pint->Set_help(
		"Amount of memory DOSBox has in kilobytes.\n"
		"  This value should normally be set to 0.\n"
		"  If nonzero, overrides the memsize parameter.\n"
		"  Finer grained control of total memory may be useful in\n"
		"  emulating ancient DOS machines with less than 640KB of\n"
		"  RAM or early 386 systems with odd extended memory sizes.\n");

	Pint = secprop->Add_int("memalias", Property::Changeable::WhenIdle,0);
	Pint->SetMinMax(0,32);
	Pint->Set_help(
		"Memory aliasing emulation, in number of valid address bits.\n"
		". Many 386/486 class motherboards and processors prior to 1995\n"
		"  suffered from memory aliasing for various technical reasons. If the software you are\n"
		"  trying to run assumes aliasing, or otherwise plays cheap tricks with paging,\n"
		"  enabling this option can help. Note that enabling this option can cause slight performance degredation. Set to 0 to disable.\n"
		"  Recommended values when enabled:\n"
		"    24: 16MB aliasing. Common on 386SX systems (CPU had 24 external address bits)\n"
		"        or 386DX and 486 systems where the CPU communicated directly with the ISA bus (A24-A31 tied off)\n"
		"    26: 64MB aliasing. Some 486s had only 26 external address bits, some motherboards tied off A26-A31\n");

	Pint = secprop->Add_int("vga bios size override", Property::Changeable::WhenIdle,0);
	Pint->SetMinMax(512,65536);
	Pint->Set_help("VGA BIOS size override. Override the size of the VGA BIOS (normally 32KB in compatible or 12KB in non-compatible).");

	Pstring = secprop->Add_string("forcerate",Property::Changeable::Always,"");
	Pstring->Set_help("Force the VGA framerate to a specific value(ntsc, pal, or specific hz), no matter what");

	Pbool = secprop->Add_bool("cgasnow",Property::Changeable::WhenIdle,true);
	Pbool->Set_help("When machine=cga, determines whether or not to emulate CGA snow");

	secprop->AddInitFunction(&CALLBACK_Init);
	secprop->AddInitFunction(&PIC_Init);//done
	secprop->AddInitFunction(&PROGRAMS_Init);
	secprop->AddInitFunction(&TIMER_Init);//done
	secprop->AddInitFunction(&CMOS_Init);//done
	secprop->AddInitFunction(&VGA_Init);

	Pmulti_remain = secprop->Add_multiremain("pit hack",Property::Changeable::Always,"");
	Pmulti_remain->Set_help(
		"If set, demo/game-specific hacks are applied to PIT timer emulation to help\n"
		"stabilize the demo and run more reliably. Valid values are:\n"
		"  'project_angel_demo'            If you intend to run the Project Angel demo, use this\n"
		"                                  setting. The PIT timer is forced to one of two values\n"
		"                                  to resolve hangups, timing issues, music skipping on\n"
		"                                  video mode changes, and VGA tearlines.\n"
		"  'pc_speaker_as_timer'           A few early DOS demos apparently like to use PIT 2 as\n"
		"                                  a timer source (where normally PIT 2 is used to generate\n"
		"                                  a square wave to drive the PC speaker). If the demo you\n"
		"                                  are running seems to run at half the normal speed for no\n"
		"                                  logical reason, try this hack. Demos that need this hack:\n"
		"                                     - Impact Studios, Legend");

	const char* pit_hax[] = { "","project_angel_demo", "pc_speaker_as_timer", 0 };
	Pstring = Pmulti_remain->GetSection()->Add_string("type",Property::Changeable::Always,"");
	Pmulti_remain->SetValue("");
	Pstring->Set_values(pit_hax);

	secprop=control->AddSection_prop("render",&RENDER_Init,true);
	Pint = secprop->Add_int("frameskip",Property::Changeable::Always,0);
	Pint->SetMinMax(0,10);
	Pint->Set_help("How many frames DOSBox skips before drawing one.");

	Pbool = secprop->Add_bool("aspect",Property::Changeable::Always,false);
	Pbool->Set_help("Do aspect correction, if your output method doesn't support scaling this can slow things down!.");

	Pbool = secprop->Add_bool("linewise",Property::Changeable::Always,true);
	Pbool->Set_help("Draw the display line by line. Needed for certain special graphics effects in games and demos. Can be changed at runtime but will be put in effect at the next mode switch.");

	Pbool = secprop->Add_bool("char9",Property::Changeable::Always,true);
	Pbool->Set_help("Allow 9-pixel wide text mode fonts.");

	/* NTS: In the original code borrowed from yhkong, this was named "multiscan". All it really does is disable
	 *      the doublescan down-rezzing DOSBox normally does with 320x240 graphics so that you get the full rendition of what a VGA output would emit. */
	Pbool = secprop->Add_bool("doublescan",Property::Changeable::Always,true);
	Pbool->Set_help("If set, doublescanned output emits two scanlines for each source line, in the\n"
			"same manner as the actual VGA output (320x200 is rendered as 640x400 for example).\n"
			"If clear, doublescanned output is rendered at the native source resolution (320x200 as 320x200).\n"
			"This affects the raster PRIOR to the software or hardware scalers. Choose wisely.\n");

	Pmulti = secprop->Add_multi("scaler",Property::Changeable::Always," ");
	Pmulti->SetValue("normal2x");
	Pmulti->Set_help("Scaler used to enlarge/enhance low resolution modes. If 'forced' is appended,\n"
	                 "then the scaler will be used even if the result might not be desired.");
	Pstring = Pmulti->GetSection()->Add_string("type",Property::Changeable::Always,"normal2x");

	const char *scalers[] = { 
		"none", "normal2x", "normal3x", "normal4x", "normal5x",
#if RENDER_USE_ADVANCED_SCALERS>2
		"advmame2x", "advmame3x", "advinterp2x", "advinterp3x", "hq2x", "hq3x", "2xsai", "super2xsai", "supereagle",
#endif
#if RENDER_USE_ADVANCED_SCALERS>0
		"tv2x", "tv3x", "rgb2x", "rgb3x", "scan2x", "scan3x",
#endif
		"hardware_none", "hardware2x", "hardware3x", "hardware4x", "hardware5x",
		0 };
	Pstring->Set_values(scalers);

	const char* force[] = { "", "forced", 0 };
	Pstring = Pmulti->GetSection()->Add_string("force",Property::Changeable::Always,"");
	Pstring->Set_values(force);

	Pbool = secprop->Add_bool("autofit",Property::Changeable::Always,true);
	Pbool->Set_help(
		"Best fits image to window\n"
		"- Intended for output=direct3d, fullresolution=original, aspect=true");


	secprop=control->AddSection_prop("vsync",&VGA_VsyncInit,true);//done
	const char* vsyncmode[] = { "off", "on" ,"force", "host", 0 };
	Pstring = secprop->Add_string("vsyncmode",Property::Changeable::WhenIdle,"off");
	Pstring->Set_values(vsyncmode);
	Pstring->Set_help("Synchronize vsync timing to the host display. Requires calibration within dosbox.");
	const char* vsyncrate[] = { "%u", 0 };
	Pstring = secprop->Add_string("vsyncrate",Property::Changeable::WhenIdle,"75");
	Pstring->Set_values(vsyncrate);
	Pstring->Set_help("Vsync rate used if vsync is enabled. Ignored if vsyncmode is set to host (win32).");

	secprop=control->AddSection_prop("cpu",&CPU_Init,true);//done
	const char* cores[] = { "auto",
#if (C_DYNAMIC_X86)
		"dynamic",
#endif
		"normal", "full", "simple", 0 };
	Pstring = secprop->Add_string("core",Property::Changeable::WhenIdle,"auto");
	Pstring->Set_values(cores);
	Pstring->Set_help("CPU Core used in emulation. auto will switch to dynamic if available and\n"
		"appropriate.");

	const char* cputype_values[] = {"auto", "386", "486", "pentium", "386_prefetch", "pentium_mmx", 0};
	Pstring = secprop->Add_string("cputype",Property::Changeable::Always,"auto");
	Pstring->Set_values(cputype_values);
	Pstring->Set_help("CPU Type used in emulation. auto emulates a 486 which tolerates Pentium instructions.");

	Pmulti_remain = secprop->Add_multiremain("cycles",Property::Changeable::Always," ");
	Pmulti_remain->Set_help(
		"Amount of instructions DOSBox tries to emulate each millisecond.\n"
		"Setting this value too high results in sound dropouts and lags.\n"
		"Cycles can be set in 3 ways:\n"
		"  'auto'          tries to guess what a game needs.\n"
		"                  It usually works, but can fail for certain games.\n"
		"  'fixed #number' will set a fixed amount of cycles. This is what you usually\n"
		"                  need if 'auto' fails (Example: fixed 4000).\n"
		"  'max'           will allocate as much cycles as your computer is able to\n"
		"                  handle.");

	const char* cyclest[] = { "auto","fixed","max","%u",0 };
	Pstring = Pmulti_remain->GetSection()->Add_string("type",Property::Changeable::Always,"auto");
	Pmulti_remain->SetValue("auto");
	Pstring->Set_values(cyclest);

	Pstring = Pmulti_remain->GetSection()->Add_string("parameters",Property::Changeable::Always,"");
	
	Pint = secprop->Add_int("cycleup",Property::Changeable::Always,10);
	Pint->SetMinMax(1,1000000);
	Pint->Set_help("Amount of cycles to decrease/increase with keycombos.(CTRL-F11/CTRL-F12)");

	Pint = secprop->Add_int("cycledown",Property::Changeable::Always,20);
	Pint->SetMinMax(1,1000000);
	Pint->Set_help("Setting it lower than 100 will be a percentage.");

	Pbool = secprop->Add_bool("ignore opcode 63",Property::Changeable::Always,true);
	Pbool->Set_help("When debugging, do not report illegal opcode 0x63.\n"
			"Enable this option to ignore spurious errors while debugging from within Windows 3.1/9x/ME");

	Pbool = secprop->Add_bool("apmbios",Property::Changeable::WhenIdle,false);
	Pbool->Set_help("Emulate Advanced Power Management BIOS calls.\n"
		"Do not enable if you are running Windows ME.");

	Pbool = secprop->Add_bool("isapnpbios",Property::Changeable::WhenIdle,true);
	Pbool->Set_help("Emulate ISA Plug & Play BIOS. Enable if using DOSBox to run a PnP aware DOS program or if booting Windows 9x.\n"
			"Do not disable if Windows 9x is configured around PnP devices, you will likely confuse it.");

	Pbool = secprop->Add_bool("realbig16",Property::Changeable::WhenIdle,false);
	Pbool->Set_help("Allow the B (big) bit in real mode. If set, allow the DOS program to set the B bit,\n"
		"then jump to realmode with B still set (aka Huge Unreal mode). Needed for Project Angel.");

#if C_FPU
	secprop->AddInitFunction(&FPU_Init);
#endif
	secprop->AddInitFunction(&DMA_Init);//done
	//secprop->AddInitFunction(&KEYBOARD_Init);
	secprop->AddInitFunction(&ISAPNP_Cfg_Init);

	secprop=control->AddSection_prop("keyboard",&KEYBOARD_Init);
	Pbool = secprop->Add_bool("aux",Property::Changeable::OnlyAtStart,false);
	Pbool->Set_help("Enable emulation of the 8042 auxiliary port. PS/2 mouse emulation requires this to be enabled.\n"
			"You should enable this if you will be running Windows ME or any other OS that does not use the BIOS to receive mouse events.");

	const char *auxdevices[] = {"none","2button","3button","intellimouse","intellimouse45",0};
	Pstring = secprop->Add_string("auxdevice",Property::Changeable::OnlyAtStart,"intellimouse");
	Pstring->Set_values(auxdevices);
	Pstring->Set_help("Type of PS/2 mouse attached to the AUX port");

#if defined(PCI_FUNCTIONALITY_ENABLED)
	secprop=control->AddSection_prop("pci",&PCI_Init,false); //PCI bus

	secprop->AddInitFunction(&VOODOO_Init,true);
	const char* voodoo_settings[] = {
		"false",
		"software",
		"auto",
		0
	};
	Pstring = secprop->Add_string("voodoo",Property::Changeable::WhenIdle,"auto");
	Pstring->Set_values(voodoo_settings);
	Pstring->Set_help("Enable VOODOO support.");
#endif


	secprop=control->AddSection_prop("mixer",&MIXER_Init);
	Pbool = secprop->Add_bool("nosound",Property::Changeable::OnlyAtStart,false);
	Pbool->Set_help("Enable silent mode, sound is still emulated though.");

   Pbool = secprop->Add_bool("swapstereo",Property::Changeable::OnlyAtStart,false); 
   Pbool->Set_help("Swaps the left and right stereo channels."); 

	Pint = secprop->Add_int("rate",Property::Changeable::OnlyAtStart,44100);
	Pint->Set_values(rates);
	Pint->Set_help("Mixer sample rate, setting any device's rate higher than this will probably lower their sound quality.");

	const char *blocksizes[] = {
		 "1024", "2048", "4096", "8192", "512", "256", 0};
	Pint = secprop->Add_int("blocksize",Property::Changeable::OnlyAtStart,1024);
	Pint->Set_values(blocksizes);
	Pint->Set_help("Mixer block size, larger blocks might help sound stuttering but sound will also be more lagged.");

	Pint = secprop->Add_int("prebuffer",Property::Changeable::OnlyAtStart,20);
	Pint->SetMinMax(0,100);
	Pint->Set_help("How many milliseconds of data to keep on top of the blocksize.");

	secprop=control->AddSection_prop("midi",&MIDI_Init,true);//done
	secprop->AddInitFunction(&MPU401_Init,true);//done
	
	const char* mputypes[] = { "intelligent", "uart", "none",0};
	// FIXME: add some way to offer the actually available choices.
	const char *devices[] = { "default", "win32", "alsa", "oss", "coreaudio", "coremidi", "mt32", "synth", "timidity", "none", 0};
	Pstring = secprop->Add_string("mpu401",Property::Changeable::WhenIdle,"intelligent");
	Pstring->Set_values(mputypes);
	Pstring->Set_help("Type of MPU-401 to emulate.");

	Pstring = secprop->Add_string("mididevice",Property::Changeable::WhenIdle,"default");
	Pstring->Set_values(devices);
	Pstring->Set_help("Device that will receive the MIDI data from MPU-401.");

	Pstring = secprop->Add_string("midiconfig",Property::Changeable::WhenIdle,"");
	Pstring->Set_help("Special configuration options for the device driver. This is usually the id of the device you want to use.\n"
	                  "  or in the case of coreaudio, you can specify a soundfont here.\n"
	                  "  When using a Roland MT-32 rev. 0 as midi output device, some games may require a delay in order to prevent 'buffer overflow' issues.\n"
	                  "  In that case, add 'delaysysex', for example: midiconfig=2 delaysysex\n"
	                  "  See the README/Manual for more details.");

	const char *mt32ReverseStereo[] = {"off", "on",0};
	Pstring = secprop->Add_string("mt32.reverse.stereo",Property::Changeable::WhenIdle,"off");
	Pstring->Set_values(mt32ReverseStereo);
	Pstring->Set_help("Reverse stereo channels for MT-32 output");

	const char *mt32log[] = {"off", "on",0};
	Pstring = secprop->Add_string("mt32.verbose",Property::Changeable::WhenIdle,"off");
	Pstring->Set_values(mt32log);
	Pstring->Set_help("MT-32 debug logging");

	const char *mt32thread[] = {"off", "on",0};
	Pstring = secprop->Add_string("mt32.thread",Property::Changeable::WhenIdle,"off");
	Pstring->Set_values(mt32thread);
	Pstring->Set_help("MT-32 rendering in separate thread");

	const char *mt32DACModes[] = {"0", "1", "2", "3", "auto",0};
	Pstring = secprop->Add_string("mt32.dac",Property::Changeable::WhenIdle,"auto");
	Pstring->Set_values(mt32DACModes);
	Pstring->Set_help("MT-32 DAC input emulation mode\n"
		"Nice = 0 - default\n"
		"Produces samples at double the volume, without tricks.\n"
		"Higher quality than the real devices\n\n"

		"Pure = 1\n"
		"Produces samples that exactly match the bits output from the emulated LA32.\n"
		"Nicer overdrive characteristics than the DAC hacks (it simply clips samples within range)\n"
		"Much less likely to overdrive than any other mode.\n"
		"Half the volume of any of the other modes, meaning its volume relative to the reverb\n"
		"output when mixed together directly will sound wrong. So, reverb level must be lowered.\n"
		"Perfect for developers while debugging :)\n\n"

		"GENERATION1 = 2\n"
		"Re-orders the LA32 output bits as in early generation MT-32s (according to Wikipedia).\n"
		"Bit order at DAC (where each number represents the original LA32 output bit number, and XX means the bit is always low):\n"
		"15 13 12 11 10 09 08 07 06 05 04 03 02 01 00 XX\n\n"

		"GENERATION2 = 3\n"
		"Re-orders the LA32 output bits as in later geneerations (personally confirmed on my CM-32L - KG).\n"
		"Bit order at DAC (where each number represents the original LA32 output bit number):\n"
		"15 13 12 11 10 09 08 07 06 05 04 03 02 01 00 14\n");

	const char *mt32reverbModes[] = {"0", "1", "2", "3", "auto",0};
	Pstring = secprop->Add_string("mt32.reverb.mode",Property::Changeable::WhenIdle,"auto");
	Pstring->Set_values(mt32reverbModes);
	Pstring->Set_help("MT-32 reverb mode");

	const char *mt32reverbTimes[] = {"0", "1", "2", "3", "4", "5", "6", "7",0};
	Pint = secprop->Add_int("mt32.reverb.time",Property::Changeable::WhenIdle,5);
	Pint->Set_values(mt32reverbTimes);
	Pint->Set_help("MT-32 reverb decaying time"); 

	const char *mt32reverbLevels[] = {"0", "1", "2", "3", "4", "5", "6", "7",0};
	Pint = secprop->Add_int("mt32.reverb.level",Property::Changeable::WhenIdle,3);
	Pint->Set_values(mt32reverbLevels);
	Pint->Set_help("MT-32 reverb level");

	Pint = secprop->Add_int("mt32.partials",Property::Changeable::WhenIdle,32);
	Pint->SetMinMax(0,256);
	Pint->Set_help("MT-32 max partials allowed (0-256)");

#if C_DEBUG
	secprop=control->AddSection_prop("debug",&DEBUG_Init);
#endif

	secprop=control->AddSection_prop("sblaster",&SBLASTER_Init,true);//done
	
	const char* sbtypes[] = { "sb1", "sb2", "sbpro1", "sbpro2", "sb16", "sb16vibra", "gb", "none", 0 };
	Pstring = secprop->Add_string("sbtype",Property::Changeable::WhenIdle,"sb16");
	Pstring->Set_values(sbtypes);
	Pstring->Set_help("Type of Soundblaster to emulate. gb is Gameblaster.");

	Phex = secprop->Add_hex("sbbase",Property::Changeable::WhenIdle,0x220);
	Phex->Set_values(ios);
	Phex->Set_help("The IO address of the soundblaster.");

	Pint = secprop->Add_int("irq",Property::Changeable::WhenIdle,7);
	Pint->Set_values(irqssb);
	Pint->Set_help("The IRQ number of the soundblaster.");

	Pint = secprop->Add_int("dma",Property::Changeable::WhenIdle,1);
	Pint->Set_values(dmassb);
	Pint->Set_help("The DMA number of the soundblaster.");

	Pint = secprop->Add_int("hdma",Property::Changeable::WhenIdle,5);
	Pint->Set_values(dmassb);
	Pint->Set_help("The High DMA number of the soundblaster.");

	Pbool = secprop->Add_bool("sbmixer",Property::Changeable::WhenIdle,true);
	Pbool->Set_help("Allow the soundblaster mixer to modify the DOSBox mixer.");

	const char* oplmodes[]={ "auto", "cms", "opl2", "dualopl2", "opl3", "none", "hardware", "hardwaregb", 0};
	Pstring = secprop->Add_string("oplmode",Property::Changeable::WhenIdle,"auto");
	Pstring->Set_values(oplmodes);
	Pstring->Set_help("Type of OPL emulation. On 'auto' the mode is determined by sblaster type.\n"
		"All OPL modes are Adlib-compatible, except for 'cms'. sbtype=none\n"
		"together with oplmode=cms will emulate a Gameblaster.");

	const char* oplemus[]={ "default", "compat", "fast", 0};
	Pstring = secprop->Add_string("oplemu",Property::Changeable::WhenIdle,"default");
	Pstring->Set_values(oplemus);
	Pstring->Set_help("Provider for the OPL emulation. compat might provide better quality (see oplrate as well).");

	Pint = secprop->Add_int("oplrate",Property::Changeable::WhenIdle,44100);
	Pint->Set_values(oplrates);
	Pint->Set_help("Sample rate of OPL music emulation. Use 49716 for highest quality (set the mixer rate accordingly).");

	Phex = secprop->Add_hex("hardwarebase",Property::Changeable::WhenIdle,0x220);
	Phex->Set_help("base address of the real hardware soundblaster:\n"\
		"210,220,230,240,250,260,280");
	Pbool = secprop->Add_bool("goldplay",Property::Changeable::WhenIdle,false);
	Pbool->Set_help("Enable goldplay emulation.");

	secprop=control->AddSection_prop("gus",&GUS_Init,true); //done
	Pbool = secprop->Add_bool("gus",Property::Changeable::WhenIdle,false); 	
	Pbool->Set_help("Enable the Gravis Ultrasound emulation.");

	Pint = secprop->Add_int("gusrate",Property::Changeable::WhenIdle,44100);
	Pint->Set_values(rates);
	Pint->Set_help("Sample rate of Ultrasound emulation.");

	Phex = secprop->Add_hex("gusbase",Property::Changeable::WhenIdle,0x240);
	Phex->Set_values(iosgus);
	Phex->Set_help("The IO base address of the Gravis Ultrasound.");

	Pint = secprop->Add_int("gusirq",Property::Changeable::WhenIdle,5);
	Pint->Set_values(irqsgus);
	Pint->Set_help("The IRQ number of the Gravis Ultrasound.");

	Pint = secprop->Add_int("gusdma",Property::Changeable::WhenIdle,3);
	Pint->Set_values(dmasgus);
	Pint->Set_help("The DMA channel of the Gravis Ultrasound.");

	Pstring = secprop->Add_string("ultradir",Property::Changeable::WhenIdle,"C:\\ULTRASND");
	Pstring->Set_help(
		"Path to Ultrasound directory. In this directory\n"
		"there should be a MIDI directory that contains\n"
		"the patch files for GUS playback. Patch sets used\n"
		"with Timidity should work fine.");

	const char *sidbaseno[] = { "240", "220", "260", "280", "2a0", "2c0", "2e0", "300", 0 };
	const char *qualityno[] = { "0", "1", "2", "3", 0 };

	secprop = control->AddSection_prop("innova",&INNOVA_Init,true);//done
	Pbool = secprop->Add_bool("innova",Property::Changeable::WhenIdle,false);
	Pbool->Set_help("Enable the Innovation SSI-2001 emulation.");
	Pint = secprop->Add_int("samplerate",Property::Changeable::WhenIdle,22050);
	Pint->Set_values(rates);
	Pint->Set_help("Sample rate of Innovation SSI-2001 emulation");
	Phex = secprop->Add_hex("sidbase",Property::Changeable::WhenIdle,0x280);
	Phex->Set_values(sidbaseno);
	Phex->Set_help("SID base port (typically 280h).");
	Pint = secprop->Add_int("quality",Property::Changeable::WhenIdle,0);
	Pint->Set_values(qualityno);
	Pint->Set_help("Set SID emulation quality level (0 to 3).");

	secprop = control->AddSection_prop("speaker",&PCSPEAKER_Init,true);//done
	Pbool = secprop->Add_bool("pcspeaker",Property::Changeable::WhenIdle,true);
	Pbool->Set_help("Enable PC-Speaker emulation.");

	Pint = secprop->Add_int("pcrate",Property::Changeable::WhenIdle,44100);
	Pint->Set_values(rates);
	Pint->Set_help("Sample rate of the PC-Speaker sound generation.");

	secprop->AddInitFunction(&TANDYSOUND_Init,true);//done
	const char* tandys[] = { "auto", "on", "off", 0};
	Pstring = secprop->Add_string("tandy",Property::Changeable::WhenIdle,"auto");
	Pstring->Set_values(tandys);
	Pstring->Set_help("Enable Tandy Sound System emulation. For 'auto', emulation is present only if machine is set to 'tandy'.");
	
	Pint = secprop->Add_int("tandyrate",Property::Changeable::WhenIdle,44100);
	Pint->Set_values(rates);
	Pint->Set_help("Sample rate of the Tandy 3-Voice generation.");

	secprop->AddInitFunction(&DISNEY_Init,true);//done
	
	Pbool = secprop->Add_bool("disney",Property::Changeable::WhenIdle,false);
	Pbool->Set_help("Enable Disney Sound Source emulation. (Covox Voice Master and Speech Thing compatible).");
	const char* ps1opt[] = { "on", "off", 0};
	secprop->AddInitFunction(&PS1SOUND_Init,true);//done
	Pstring = secprop->Add_string("ps1audio",Property::Changeable::WhenIdle,"off");
	Pstring->Set_values(ps1opt);
	Pstring->Set_help("Enable PS1 audio emulation.");
	Pint = secprop->Add_int("ps1audiorate",Property::Changeable::OnlyAtStart,22050);
	Pint->Set_values(rates);
	Pint->Set_help("Sample rate of the PS1 audio emulation.");

	secprop=control->AddSection_prop("joystick",&BIOS_Init,false);//done
	secprop->AddInitFunction(&INT10_Init);
	secprop->AddInitFunction(&JOYSTICK_Init);
	const char* joytypes[] = { "auto", "2axis", "4axis", "4axis_2", "fcs", "ch", "none",0};
	Pstring = secprop->Add_string("joysticktype",Property::Changeable::WhenIdle,"auto");
	Pstring->Set_values(joytypes);
	Pstring->Set_help(
		"Type of joystick to emulate: auto (default), none,\n"
		"2axis (supports two joysticks),\n"
		"4axis (supports one joystick, first joystick used),\n"
		"4axis_2 (supports one joystick, second joystick used),\n"
		"fcs (Thrustmaster), ch (CH Flightstick).\n"
		"none disables joystick emulation.\n"
		"auto chooses emulation depending on real joystick(s).\n"
		"(Remember to reset dosbox's mapperfile if you saved it earlier)");

	Pbool = secprop->Add_bool("timed",Property::Changeable::WhenIdle,true);
	Pbool->Set_help("enable timed intervals for axis. Experiment with this option, if your joystick drifts (away).");

	Pbool = secprop->Add_bool("autofire",Property::Changeable::WhenIdle,false);
	Pbool->Set_help("continuously fires as long as you keep the button pressed.");
	
	Pbool = secprop->Add_bool("swap34",Property::Changeable::WhenIdle,false);
	Pbool->Set_help("swap the 3rd and the 4th axis. can be useful for certain joysticks.");

	Pbool = secprop->Add_bool("buttonwrap",Property::Changeable::WhenIdle,false);
	Pbool->Set_help("enable button wrapping at the number of emulated buttons.");

	secprop=control->AddSection_prop("serial",&SERIAL_Init,true);
	const char* serials[] = { "dummy", "disabled", "modem", "nullmodem", "serialmouse",
	                          "directserial",0 };
   
	Pmulti_remain = secprop->Add_multiremain("serial1",Property::Changeable::WhenIdle," ");
	Pstring = Pmulti_remain->GetSection()->Add_string("type",Property::Changeable::WhenIdle,"dummy");
	Pmulti_remain->SetValue("dummy");
	Pstring->Set_values(serials);
	Pstring = Pmulti_remain->GetSection()->Add_string("parameters",Property::Changeable::WhenIdle,"");
	Pmulti_remain->Set_help(
		"set type of device connected to com port.\n"
		"Can be disabled, dummy, modem, nullmodem, directserial.\n"
		"Additional parameters must be in the same line in the form of\n"
		"parameter:value. Parameter for all types is irq (optional).\n"
		"for directserial: realport (required), rxdelay (optional).\n"
		"                 (realport:COM1 realport:ttyS0).\n"
		"for modem: listenport (optional).\n"
		"for nullmodem: server, rxdelay, txdelay, telnet, usedtr,\n"
		"               transparent, port, inhsocket (all optional).\n"
		"Example: serial1=modem listenport:5000");

	Pmulti_remain = secprop->Add_multiremain("serial2",Property::Changeable::WhenIdle," ");
	Pstring = Pmulti_remain->GetSection()->Add_string("type",Property::Changeable::WhenIdle,"dummy");
	Pmulti_remain->SetValue("dummy");
	Pstring->Set_values(serials);
	Pstring = Pmulti_remain->GetSection()->Add_string("parameters",Property::Changeable::WhenIdle,"");
	Pmulti_remain->Set_help("see serial1");

	Pmulti_remain = secprop->Add_multiremain("serial3",Property::Changeable::WhenIdle," ");
	Pstring = Pmulti_remain->GetSection()->Add_string("type",Property::Changeable::WhenIdle,"disabled");
	Pmulti_remain->SetValue("disabled");
	Pstring->Set_values(serials);
	Pstring = Pmulti_remain->GetSection()->Add_string("parameters",Property::Changeable::WhenIdle,"");
	Pmulti_remain->Set_help("see serial1");

	Pmulti_remain = secprop->Add_multiremain("serial4",Property::Changeable::WhenIdle," ");
	Pstring = Pmulti_remain->GetSection()->Add_string("type",Property::Changeable::WhenIdle,"disabled");
	Pmulti_remain->SetValue("disabled");
	Pstring->Set_values(serials);
	Pstring = Pmulti_remain->GetSection()->Add_string("parameters",Property::Changeable::WhenIdle,"");
	Pmulti_remain->Set_help("see serial1");

	// parallel ports
	secprop=control->AddSection_prop("parallel",&PARALLEL_Init,true);
	Pstring = secprop->Add_string("parallel1",Property::Changeable::WhenIdle,"disabled");
	Pstring->Set_help(
	        "parallel1-3 -- set type of device connected to lpt port.\n"
			"Can be:\n"
			"	reallpt (direct parallel port passthrough),\n"
			"	file (records data to a file or passes it to a device),\n"
			"	printer (virtual dot-matrix printer, see [printer] section)\n"
	        "Additional parameters must be in the same line in the form of\n"
	        "parameter:value.\n"
	        "  for reallpt:\n"
	        "  Windows:\n"
			"    realbase (the base address of your real parallel port).\n"
			"      Default: 378\n"
			"    ecpbase (base address of the ECP registers, optional).\n"
			"  Linux: realport (the parallel port device i.e. /dev/parport0).\n"
			"  for file: \n"
			"    dev:<devname> (i.e. dev:lpt1) to forward data to a device,\n"
			"    or append:<file> appends data to the specified file.\n"
			"    Without the above parameters data is written to files in the capture dir.\n"
			"    Additional parameters: timeout:<milliseconds> = how long to wait before\n"
			"    closing the file on inactivity (default:500), addFF to add a formfeed when\n"
			"    closing, addLF to add a linefeed if the app doesn't, cp:<codepage number>\n"
			"    to perform codepage translation, i.e. cp:437\n"
			"  for printer:\n"
			"    printer still has it's own configuration section above."
	);
	Pstring = secprop->Add_string("parallel2",Property::Changeable::WhenIdle,"disabled");
	Pstring->Set_help("see parallel1");
	Pstring = secprop->Add_string("parallel3",Property::Changeable::WhenIdle,"disabled");
	Pstring->Set_help("see parallel1");

	secprop->AddInitFunction(&DONGLE_Init,true);//done
	Pbool = secprop->Add_bool("dongle",Property::Changeable::WhenIdle,false);
	Pbool->Set_help("Enable dongle");

	/* All the DOS Related stuff, which will eventually start up in the shell */
	secprop=control->AddSection_prop("dos",&DOS_Init,false);//done
	secprop->AddInitFunction(&XMS_Init,true);//done
	Pbool = secprop->Add_bool("xms",Property::Changeable::WhenIdle,true);
	Pbool->Set_help("Enable XMS support.");

	secprop->AddInitFunction(&EMS_Init,true);//done
	const char* ems_settings[] = { "true", "emsboard", "emm386", "false", 0};
	Pstring = secprop->Add_string("ems",Property::Changeable::WhenIdle,"true");
	Pstring->Set_values(ems_settings);
	Pstring->Set_help("Enable EMS support. The default (=true) provides the best\n"
		"compatibility but certain applications may run better with\n"
		"other choices, or require EMS support to be disabled (=false)\n"
		"to work at all.");

	Pbool = secprop->Add_bool("umb",Property::Changeable::WhenIdle,true);
	Pbool->Set_help("Enable UMB support.");

	Phex = secprop->Add_hex("umb start",Property::Changeable::OnlyAtStart,0); /* <- (0=auto) 0xD000 is mainline DOSBox compatible behavior */
	Phex->Set_help("UMB region starting segment");

	Phex = secprop->Add_hex("umb end",Property::Changeable::OnlyAtStart,0); /* <- (0=auto) 0xEFFF is mainline DOSBox compatible (where base=0xD000 and size=0x2000) */
	Phex->Set_help("UMB region last segment");

	Pbool = secprop->Add_bool("dynamic kernel allocation",Property::Changeable::OnlyAtStart,false);
	Pbool->Set_help("If set, DOS kernel structures are allocated dynamically. If clear, DOS kernel structures are fixed at specific segments (mainline DOSBox behavior)");

	Pbool = secprop->Add_bool("keep umb on boot",Property::Changeable::OnlyAtStart,0);
	Pbool->Set_help("If emulating UMBs, keep the UMB around after boot (Mainline DOSBox behavior). If clear, UMB is unmapped when you boot an operating system.");

	Pbool = secprop->Add_bool("keep private area on boot",Property::Changeable::OnlyAtStart,0);
	Pbool->Set_help("If set, keep the DOSBox private area around after boot (Mainline DOSBox behavior). If clear, unmap and discard the private area when you boot an operating system.");

	Pbool = secprop->Add_bool("private area in umb",Property::Changeable::WhenIdle,true);
	Pbool->Set_help("If set, keep private DOS segment in upper memory block, usually segment 0xC800 (Mainline DOSBox behavior)\n"
			"If clear, place private DOS segment at the base of system memory (just below the MCB)");

	Pbool = secprop->Add_bool("automount",Property::Changeable::WhenIdle,true);
	Pbool->Set_help("Enable automatic mount.");

	Pbool = secprop->Add_bool("int33",Property::Changeable::WhenIdle,true);
	Pbool->Set_help("Enable INT 33H (mouse) support.");

	Pbool = secprop->Add_bool("biosps2",Property::Changeable::OnlyAtStart,true);
	Pbool->Set_help("Emulate BIOS INT 15h PS/2 mouse services\n"
		"Note that some OS's like Microsoft Windows neither use INT 33h nor\n"
		"probe the AUX port directly and depend on this BIOS interface exclusively\n"
		"for PS/2 mouse support. In other cases there is no harm in leaving this enabled");

	secprop->AddInitFunction(&MOUSE_Init); //Must be after int10 as it uses CurMode
	secprop->AddInitFunction(&DOS_KeyboardLayout_Init,true);
	Pstring = secprop->Add_string("keyboardlayout",Property::Changeable::WhenIdle, "auto");
	Pstring->Set_help("Language code of the keyboard layout (or none).");

	Pbool = secprop->Add_bool("dbcs",Property::Changeable::OnlyAtStart,true);
	Pbool->Set_help("Enable DBCS table");

	Pbool = secprop->Add_bool("filenamechar",Property::Changeable::OnlyAtStart,true);
	Pbool->Set_help("Enable filename char table");

	Pbool = secprop->Add_bool("collating and uppercase",Property::Changeable::OnlyAtStart,true);
	Pbool->Set_help("Enable collating and uppercase table");

	Pint = secprop->Add_int("files",Property::Changeable::OnlyAtStart,127);
	Pint->Set_help("Number of file handles available to DOS programs. (equivalent to \"files=\" in config.sys)");

	// Mscdex
	secprop->AddInitFunction(&MSCDEX_Init);
	secprop->AddInitFunction(&DRIVES_Init);
	secprop->AddInitFunction(&CDROM_Image_Init);
#if C_IPX
	secprop=control->AddSection_prop("ipx",&IPX_Init,true);
	Pbool = secprop->Add_bool("ipx",Property::Changeable::WhenIdle, false);
	Pbool->Set_help("Enable ipx over UDP/IP emulation.");
#endif

#if C_NE2000
	secprop=control->AddSection_prop("ne2000",&NE2K_Init,true);
	MSG_Add("NE2000_CONFIGFILE_HELP",
		"macaddr -- The physical address the emulator will use on your network.\n"
		"           If you have multiple DOSBoxes running on your network,\n"
		"           this has to be changed. Modify the last three number blocks.\n"
		"           I.e. AC:DE:48:88:99:AB.\n"
		"realnic -- Specifies which of your network interfaces is used.\n"
		"           Write \'list\' here to see the list of devices in the\n"
		"           Status Window. Then make your choice and put either the\n"
		"           interface number (2 or something) or a part of your adapters\n"
		"           name, e.g. VIA here.\n"

	);

	Pbool = secprop->Add_bool("ne2000", Property::Changeable::WhenIdle, true);
	Pbool->Set_help("Enable Ethernet passthrough. Requires [Win]Pcap.");

	Phex = secprop->Add_hex("nicbase", Property::Changeable::WhenIdle, 0x300);
	Phex->Set_help("The base address of the NE2000 board.");

	Pint = secprop->Add_int("nicirq", Property::Changeable::WhenIdle, 3);
	Pint->Set_help("The interrupt it uses. Note serial2 uses IRQ3 as default.");

	Pstring = secprop->Add_string("macaddr", Property::Changeable::WhenIdle,"AC:DE:48:88:99:AA");
	Pstring->Set_help("The physical address the emulator will use on your network.\n"
		"If you have multiple DOSBoxes running on your network,\n"
		"this has to be changed for each. AC:DE:48 is an address range reserved for\n"
		"private use, so modify the last three number blocks.\n"
		"I.e. AC:DE:48:88:99:AB.");
	
	Pstring = secprop->Add_string("realnic", Property::Changeable::WhenIdle,"list");
	Pstring->Set_help("Specifies which of your network interfaces is used.\n"
		"Write \'list\' here to see the list of devices in the\n"
		"Status Window. Then make your choice and put either the\n"
		"interface number (2 or something) or a part of your adapters\n"
		"name, e.g. VIA here.");
#endif // C_NE2000
//	secprop->AddInitFunction(&CREDITS_Init);

	/* IDE emulation options and setup */
	const char *ide_names[4] = {"ide, primary","ide, secondary","ide, tertiary","ide, quaternary"};
	void (*ide_inits[4])(Section *) = {
		&IDE_Primary_Init,
		&IDE_Secondary_Init,
		&IDE_Tertiary_Init,
		&IDE_Quaternary_Init
	};
	for (size_t i=0;i < 4;i++) {
		secprop=control->AddSection_prop(ide_names[i],ide_inits[i],false);//done

		Pbool = secprop->Add_bool("enable",Property::Changeable::OnlyAtStart,true);
		if (i == 0) Pbool->Set_help("Enable IDE interface");

		Pbool = secprop->Add_bool("int13fakeio",Property::Changeable::WhenIdle,false);
		if (i == 0) Pbool->Set_help(
				"If set, force IDE state change on certain INT 13h commands.\n"
				"IDE registers will be changed as if BIOS had carried out the action.\n"
				"If you are running Windows 3.11 or Windows 3.11 Windows for Workgroups\n"
				"you must enable this option (and use -reservecyl 1) if you want 32-bit\n"
				"disk access to work correctly in DOSBox.");

		Pbool = secprop->Add_bool("int13fakev86io",Property::Changeable::WhenIdle,false);
		if (i == 0) Pbool->Set_help(
				"If set, and int13fakeio is set, certain INT 13h commands will\n"
				"cause IDE emulation to issue fake CPU I/O traps (GPF) in\n"
				"virtual 8086 mode and a fake IRQ signal. you must enable this option\n"
				"if you want 32-bit disk access in Windows 95 to work with DOSBox.");
	}

	//TODO ?
	secline=control->AddSection_line("autoexec",&AUTOEXEC_Init);
	MSG_Add("AUTOEXEC_CONFIGFILE_HELP",
		"Lines in this section will be run at startup.\n"
		"You can put your MOUNT lines here.\n"
	);
	MSG_Add("CONFIGFILE_INTRO",
	        "# This is the configuration file for DOSBox %s. (Please use the latest version of DOSBox)\n"
	        "# Lines starting with a # are comment lines and are ignored by DOSBox.\n"
	        "# They are used to (briefly) document the effect of each option.\n");
	MSG_Add("CONFIG_SUGGESTED_VALUES", "Possible values");

	control->SetStartUp(&SHELL_Init);
}

