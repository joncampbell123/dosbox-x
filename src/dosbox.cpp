/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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

/* NTS: Valgrind hunting shows memory leak from C++ new operator somewhere
 *      with the JACK library indirectly invoked by SDL audio. Can we resolve
 *      that too eventually? */

/* NTS: Valgrind hunting also shows one of the section INIT functions (I can't
 *      yet tell which one because the stack trace doesn't show it) is allocating
 *      something and is not freeing it. */

/* NTS: Valgrind hunting has a moderate to high signal-to-noise ratio because
 *      of memory leaks (lazy memory allocation) from other libraries in the
 *      system, including:
 *
 *         ncurses
 *         libSDL
 *         libX11 and libXCB
 *         libasound (ALSA sound library)
 *         PulseAudio library calls
 *         JACK library calls
 *         libdl (the dlopen/dlclose functions allocate something and never free it)
 *         and a whole bunch of unidentified malloc calls without a matching free.
 *
 *      On my dev system, a reported leak of 450KB (77KB possibly lost + 384KB still reachable
 *      according to Valgrind) is normal.
 *
 *      Now you ask: why do I care so much about Valgrind, memory leaks, and cleaning
 *      up the code? The less spurious memory leaks, the easier it is to identify
 *      actual leaks among the noise and to patch them up. Thus, "valgrind hunting" --J.C. */

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
#include "ide.h"
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
#include "clockdomain.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <list>

/*===================================TODO: Move to it's own file==============================*/
#if defined(__SSE__) && !defined(_M_AMD64)
bool sse2_available = false;

# ifdef __GNUC__
#  define cpuid(func,ax,bx,cx,dx)\
    __asm__ __volatile__ ("cpuid":\
    "=a" (ax), "=b" (bx), "=c" (cx), "=d" (dx) : "a" (func));
# endif /* __GNUC__ */

# if defined(_MSC_VER)
#  define cpuid(func,a,b,c,d)\
    __asm mov eax, func\
    __asm cpuid\
    __asm mov a, eax\
    __asm mov b, ebx\
    __asm mov c, ecx\
    __asm mov d, edx
# endif /* _MSC_VER */

void CheckSSESupport()
{
#if (defined (__GNUC__) || (_MSC_VER))
    Bitu a, b, c, d;
    cpuid(1, a, b, c, d);
    sse2_available = ((d >> 26) & 1)?true:false;
#endif
}
#endif
/*=============================================================================*/

extern void         GFX_SetTitle(Bit32s cycles,Bits frameskip,Bits timing,bool paused);

extern Bitu         frames;
extern Bitu         cycle_count;
extern bool         sse2_available;
extern bool         dynamic_dos_kernel_alloc;
extern Bitu         DOS_PRIVATE_SEGMENT_Size;
extern bool         VGA_BIOS_dont_duplicate_CGA_first_half;
extern bool         VIDEO_BIOS_always_carry_14_high_font;
extern bool         VIDEO_BIOS_always_carry_16_high_font;
extern bool         VIDEO_BIOS_enable_CGA_8x8_second_half;
extern bool         allow_more_than_640kb;

Bit32u              guest_msdos_LoL = 0;
Bit16u              guest_msdos_mcb_chain = 0;
int                 boothax = BOOTHAX_NONE;

bool                dos_con_use_int16_to_detect_input = true;

bool                dbg_zero_on_dos_allocmem = true;
bool                dbg_zero_on_xms_allocmem = true;
bool                dbg_zero_on_ems_allocmem = true;

Config*             control;
MachineType         machine;
SVGACards           svgaCard;
bool                SDLNetInited;
Bit32s              ticksDone;
Bit32u              ticksScheduled;
bool                ticksLocked;
bool                mono_cga=false;
bool                ignore_opcode_63 = true;
int             dynamic_core_cache_block_size = 32;
Bitu                VGA_BIOS_Size_override = 0;
Bitu                VGA_BIOS_SEG = 0xC000;
Bitu                VGA_BIOS_SEG_END = 0xC800;
Bitu                VGA_BIOS_Size = 0x8000;

static Bit32u           ticksRemain;
static Bit32u           ticksLast;
static Bit32u           ticksLastFramecounter;
static Bit32u           ticksLastRTcounter;
static double           ticksLastRTtime;
static Bit32u           ticksAdded;
static Bit32u           Ticks = 0;
extern double           rtdelta;
static LoopHandler*     loop;

/* The whole load of startups for all the subfunctions */
void                LOG_StartUp(void);
void                MEM_Init(Section *);
void                ISAPNP_Cfg_Init(Section *);
void                ROMBIOS_Init(Section *);
void                CALLBACK_Init(Section*);
void                PROGRAMS_Init(Section*);
void                RENDER_Init(Section*);
void                VGA_VsyncInit(Section*);
void                VGA_Init(Section*);
void                DOS_Init(Section*);
void                CPU_Init(Section*);
#if C_FPU
void                FPU_Init(Section*);
#endif
void                DMA_Init(Section*);
void                MIXER_Init(Section*);
void                MIDI_Init(Section*);
void                HARDWARE_Init(Section*);

void                IDE_Primary_Init(Section*);
void                IDE_Secondary_Init(Section*);
void                IDE_Tertiary_Init(Section*);
void                IDE_Quaternary_Init(Section*);
void                IDE_Quinternary_Init(Section*);
void                IDE_Sexternary_Init(Section*);
void                IDE_Septernary_Init(Section*);
void                IDE_Octernary_Init(Section*);

void                FDC_Primary_Init(Section*);

void                KEYBOARD_Init(Section*);    //TODO This should setup INT 16 too but ok ;)
void                JOYSTICK_Init(Section*);
void                MOUSE_Init(Section*);
void                SBLASTER_Init(Section*);
void                GUS_Init(Section*);
void                MPU401_Init(Section*);
void                PCSPEAKER_Init(Section*);
void                TANDYSOUND_Init(Section*);
void                DISNEY_Init(Section*);
void                PS1SOUND_Init(Section*);
void                INNOVA_Init(Section*);
void                SERIAL_Init(Section*); 
void                DONGLE_Init(Section*);
void                SID_Init(Section* sec);
void                PIC_Init(Section*);
void                TIMER_Init(Section*);
void                BIOS_Init(Section*);
void                DEBUG_Init(Section*);
void                CMOS_Init(Section*);
void                MSCDEX_Init(Section*);
void                DRIVES_Init(Section*);
void                CDROM_Image_Init(Section*);
void                EMS_Init(Section*);
void                XMS_Init(Section*);
void                DOS_KeyboardLayout_Init(Section*);
void                AUTOEXEC_Init(Section*);
void                INT10_Init(Section*);

signed long long time_to_clockdom(ClockDomain &src,double t) {
    signed long long lt = (signed long long)t;

    lt *= (signed long long)src.freq;
    lt /= (signed long long)src.freq_div;
    return lt;
}

unsigned long long update_clockdom_from_now(ClockDomain &dst) {
    signed long long s;

    /* PIC_Ticks (if I read the code correctly) is millisecond ticks, units of 1/1000 seconds.
     * PIC_TickIndexND() units of submillisecond time in units of 1/CPU_CycleMax. */
    s  = (signed long long)((unsigned long long)PIC_Ticks * (unsigned long long)dst.freq);
    s += (signed long long)(((unsigned long long)PIC_TickIndexND() * (unsigned long long)dst.freq) / (unsigned long long)CPU_CycleMax);
    /* convert down to frequency counts, not freq x 1000 */
    s /= (signed long long)(1000ULL * (unsigned long long)dst.freq_div);

    /* guard against time going backwards slightly (as PIC_TickIndexND() will do sometimes by tiny amounts) */
    if (dst.counter < (unsigned long long)s) dst.counter = (unsigned long long)s;

    return dst.counter;
}

#include "paging.h"

extern bool rom_bios_vptable_enable;
extern bool rom_bios_8x8_cga_font;
extern bool allow_port_92_reset;
extern bool allow_keyb_reset;

extern bool DOSBox_Paused();

//#define DEBUG_CYCLE_OVERRUN_CALLBACK

static Bitu Normal_Loop(void) {
    bool saved_allow = dosbox_allow_nonrecursive_page_fault;
    Bit32u ticksNew;
    Bits ret;

    if (!menu.hidecycles || menu.showrt) { /* sdlmain.cpp/render.cpp doesn't even maintain the frames count when hiding cycles! */
        ticksNew = GetTicks();
        if (ticksNew >= Ticks) {
            Bit32u interval = ticksNew - ticksLastFramecounter;
            double rtnow = PIC_FullIndex();

            if (interval == 0) interval = 1; // avoid divide by zero

            rtdelta = rtnow - ticksLastRTtime;
            rtdelta = (rtdelta * 1000) / interval;

            ticksLastRTtime = rtnow;
            ticksLastFramecounter = Ticks;
            Ticks = ticksNew + 500;     // next update in 500ms
            frames = (frames * 1000) / interval; // compensate for interval, be more exact (FIXME: so can we adjust for fractional frame rates)
            GFX_SetTitle(CPU_CycleMax,-1,-1,false);
            frames = 0;
        }
    }

    try {
        while (1) {
            if (PIC_RunQueue()) {
                /* now is the time to check for the NMI (Non-maskable interrupt) */
                CPU_Check_NMI();

                saved_allow = dosbox_allow_nonrecursive_page_fault;
                dosbox_allow_nonrecursive_page_fault = true;
                ret = (*cpudecoder)();
                dosbox_allow_nonrecursive_page_fault = saved_allow;

                if (GCC_UNLIKELY(ret<0))
                    return 1;

                if (ret>0) {
                    if (GCC_UNLIKELY((unsigned int)ret >= CB_MAX))
                        return 0;

                    extern unsigned int last_callback;
                    unsigned int p_last_callback = last_callback;
                    last_callback = ret;

                    dosbox_allow_nonrecursive_page_fault = false;
                    Bitu blah = (*CallBack_Handlers[ret])();
                    dosbox_allow_nonrecursive_page_fault = saved_allow;

                    last_callback = p_last_callback;

#ifdef DEBUG_CYCLE_OVERRUN_CALLBACK
                    {
                        extern char* CallBack_Description[CB_MAX];

                        /* I/O delay can cause negative CPU_Cycles and PIC event / audio rendering issues */
                        cpu_cycles_count_t overrun = -std::min(CPU_Cycles,(cpu_cycles_count_t)0);

                        if (overrun > (CPU_CycleMax/100))
                            LOG_MSG("Normal loop: CPU cycles count overrun by %ld (%.3fms) after callback '%s'\n",(signed long)overrun,(double)overrun / CPU_CycleMax,CallBack_Description[ret]);
                    }
#endif

                    if (GCC_UNLIKELY(blah > 0U))
                        return blah;
                }
#if C_DEBUG
                if (DEBUG_ExitLoop())
                    return 0;
#endif
            } else {
                GFX_Events();
                if (DOSBox_Paused() == false && ticksRemain > 0) {
                    TIMER_AddTick();
                    ticksRemain--;
                } else {
                    goto increaseticks;
                }
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
            ticksNew=GetTicks();
            ticksScheduled += ticksAdded;
            if (ticksNew > ticksLast) {
                ticksRemain = ticksNew-ticksLast;
                ticksLast = ticksNew;
                ticksDone += (Bit32s)ticksRemain;
                if ( ticksRemain > 20 ) {
                    ticksRemain = 20;
                }
                ticksAdded = ticksRemain;
            } else {
                ticksAdded = 0;
                SDL_Delay(1);
                ticksDone -= (Bit32s)((Bit32u)(GetTicks() - ticksNew));
                if (ticksDone < 0)
                    ticksDone = 0;
            }
        }
    }
    catch (GuestPageFaultException &pf) {
        Bitu FillFlags(void);

        ret = 0;
        FillFlags();
        dosbox_allow_nonrecursive_page_fault = false;
        CPU_Exception(EXCEPTION_PF,pf.faultcode);
        dosbox_allow_nonrecursive_page_fault = saved_allow;
    }
    catch (int x) {
        dosbox_allow_nonrecursive_page_fault = saved_allow;
        if (x == 4/*CMOS shutdown*/) {
            ret = 0;
//          LOG_MSG("CMOS shutdown reset acknowledged");
        }
        else {
            throw;
        }
    }

    return 0;
}

LoopHandler *DOSBOX_GetLoop(void) {
    return loop;
}

void DOSBOX_SetLoop(LoopHandler * handler) {
    loop=handler;
}

void DOSBOX_SetNormalLoop() {
    loop=Normal_Loop;
}

//#define DEBUG_RECURSION

#ifdef DEBUG_RECURSION
volatile int runmachine_recursion = 0;
#endif

void DOSBOX_RunMachine(void){
    Bitu ret;

    extern unsigned int last_callback;
    unsigned int p_last_callback = last_callback;
    last_callback = 0;

#ifdef DEBUG_RECURSION
    if (runmachine_recursion++ != 0)
        LOG_MSG("RunMachine recursion");
#endif

    do {
        ret=(*loop)();
    } while (!ret);

#ifdef DEBUG_RECURSION
    if (--runmachine_recursion < 0)
        LOG_MSG("RunMachine recursion leave error");
#endif

    last_callback = p_last_callback;
}

static void DOSBOX_UnlockSpeed( bool pressed ) {
    if (pressed) {
        LOG_MSG("Fast Forward ON");
        ticksLocked = true;
    } else {
        LOG_MSG("Fast Forward OFF");
        ticksLocked = false;
    }
    GFX_SetTitle(-1,-1,-1,false);
}

void DOSBOX_UnlockSpeed2( bool pressed ) {
    if (pressed) {
        ticksLocked =! ticksLocked;
        DOSBOX_UnlockSpeed(ticksLocked?true:false);

        /* make sure the menu item keeps up with our state */
        mainMenu.get_item("mapper_speedlock2").check(ticksLocked).refresh_item(mainMenu);
    }
}

void notifyError(const std::string& message)
{
#ifdef WIN32
    ::MessageBox(0, message.c_str(), "Error", 0);
#endif
    LOG_MSG("%s",message.c_str());
}

/* TODO: move to utility header */
#ifdef _MSC_VER /* Microsoft C++ does not have strtoull */
# if _MSC_VER < 1800 /* But Visual Studio 2013 apparently does (http://www.vogons.org/viewtopic.php?f=41&t=31881&sid=49ff69ebc0459ed6523f5a250daa4d8c&start=400#p355770) */
unsigned long long strtoull(const char *s,char **endptr,int base) {
    return _strtoui64(s,endptr,base); /* pfff... whatever Microsoft */
}
# endif
#endif

/* utility function. rename as appropriate and move to utility collection */
void parse_busclk_setting_str(ClockDomain *cd,const char *s) {
    const char *d;

    /* we're expecting an integer, a float, or an integer ratio */
    d = strchr(s,'/');
    if (d != NULL) { /* it has a slash therefore an integer ratio */
        unsigned long long num,den;

        while (*d == ' ' || *d == '/') d++;
        num = strtoull(s,NULL,0);
        den = strtoull(d,NULL,0);
        if (num >= 1ULL && den >= 1ULL) cd->set_frequency(num,den);
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

            if (fi >= 1ULL) cd->set_frequency(fi,den);
        }
        else {
            unsigned long long f = strtoull(s,NULL,10);
            if (f >= 1ULL) cd->set_frequency(f,1);
        }
    }
}

void clocktree_build_conversion_list();

void Null_Init(Section *sec) {
	(void)sec;
}

extern Bit8u cga_comp;
extern bool new_cga;

bool dpi_aware_enable = true;

void DOSBOX_InitTickLoop() {
    LOG(LOG_MISC,LOG_DEBUG)("Initializing tick loop management");

    ticksRemain = 0;
    ticksLocked = false;
    ticksLastRTtime = 0;
    ticksLast = GetTicks();
    ticksLastRTcounter = GetTicks();
    ticksLastFramecounter = GetTicks();
    DOSBOX_SetLoop(&Normal_Loop);
}

void Init_VGABIOS() {
    Section_prop *section = static_cast<Section_prop *>(control->GetSection("dosbox"));
    assert(section != NULL);

    if (IS_PC98_ARCH) {
        // There IS no VGA BIOS, this is PC-98 mode!
        VGA_BIOS_SEG = 0xC000;
        VGA_BIOS_SEG_END = 0xC000; // Important: DOS kernel uses this to determine where to place the private area!
        VGA_BIOS_Size = 0;
        return;
    }

    // log
    LOG(LOG_MISC,LOG_DEBUG)("Init_VGABIOS: Initializing VGA BIOS and parsing it's settings");

    // mem init must have already happened.
    // We can remove this once the device callout system is in place.
    assert(MemBase != NULL);

    VGA_BIOS_Size_override = (Bitu)section->Get_int("vga bios size override");
    if (VGA_BIOS_Size_override > 0) VGA_BIOS_Size_override = (VGA_BIOS_Size_override+0x7FFU)&(~0xFFFU);

    VGA_BIOS_dont_duplicate_CGA_first_half = section->Get_bool("video bios dont duplicate cga first half rom font");
    VIDEO_BIOS_always_carry_14_high_font = section->Get_bool("video bios always offer 14-pixel high rom font");
    VIDEO_BIOS_always_carry_16_high_font = section->Get_bool("video bios always offer 16-pixel high rom font");
    VIDEO_BIOS_enable_CGA_8x8_second_half = section->Get_bool("video bios enable cga second half rom font");
    /* NTS: mainline compatible mapping demands the 8x8 CGA font */
    rom_bios_8x8_cga_font = section->Get_bool("rom bios 8x8 CGA font");
    rom_bios_vptable_enable = section->Get_bool("rom bios video parameter table");

    /* sanity check */
    if (VGA_BIOS_dont_duplicate_CGA_first_half && !rom_bios_8x8_cga_font) /* can't point at the BIOS copy if it's not there */
        VGA_BIOS_dont_duplicate_CGA_first_half = false;

    if (VGA_BIOS_Size_override >= 512 && VGA_BIOS_Size_override <= 65536)
        VGA_BIOS_Size = (VGA_BIOS_Size_override + 0x7FFU) & (~0xFFFU);
    else if (IS_VGA_ARCH) {
        if (svgaCard == SVGA_S3Trio)
            VGA_BIOS_Size = 0x4000;
        else
            VGA_BIOS_Size = 0x3000;
    }
    else if (machine == MCH_EGA) {
        if (VIDEO_BIOS_always_carry_16_high_font)
            VGA_BIOS_Size = 0x3000;
        else
            VGA_BIOS_Size = 0x2000;
    }
    else {
        if (VIDEO_BIOS_always_carry_16_high_font && VIDEO_BIOS_always_carry_14_high_font)
            VGA_BIOS_Size = 0x3000;
        else if (VIDEO_BIOS_always_carry_16_high_font || VIDEO_BIOS_always_carry_14_high_font)
            VGA_BIOS_Size = 0x2000;
        else
            VGA_BIOS_Size = 0;
    }
    VGA_BIOS_SEG = 0xC000;
    VGA_BIOS_SEG_END = (VGA_BIOS_SEG + (VGA_BIOS_Size >> 4));

    /* clear for VGA BIOS (FIXME: Why does Project Angel like our BIOS when we memset() here, but don't like it if we memset() in the INT 10 ROM setup routine?) */
    if (VGA_BIOS_Size != 0)
        memset((char*)MemBase+0xC0000,0x00,VGA_BIOS_Size);
}

void DOSBOX_RealInit() {
    DOSBoxMenu::item *item;

    LOG(LOG_MISC,LOG_DEBUG)("DOSBOX_RealInit: loading settings and initializing");

    MAPPER_AddHandler(DOSBOX_UnlockSpeed, MK_rightarrow, MMODHOST,"speedlock","Speedlock");
    {
        MAPPER_AddHandler(DOSBOX_UnlockSpeed2, MK_nothing, 0, "speedlock2", "Speedlock2", &item);
        item->set_description("Toggle emulation speed, to allow running faster than realtime (fast forward)");
        item->set_text("Turbo (Fast Forward)");
    }

    Section_prop *section = static_cast<Section_prop *>(control->GetSection("dosbox"));
    assert(section != NULL);

    // TODO: a bit of a challenge: if we put it in the ROM area as mainline DOSBox does then the init
    //       needs to read this from the BIOS where it can map the memory appropriately. if the allocation
    //       is dynamic and the private area is down at the base of memory like real DOS, then the BIOS
    //       should ignore it and the DOS kernel should parse it. If we're going to put it into upper
    //       areas as well, then we should also consider making it look like adapter ROM at startup
    //       so it can be enumerated properly by DOS programs scanning the ROM area.
    /* private area size param in bytes. round up to nearest paragraph */
    DOS_PRIVATE_SEGMENT_Size = 32768 / 16;

    // TODO: these should be parsed by BIOS startup
    allow_more_than_640kb = section->Get_bool("allow more than 640kb base memory");

    // TODO: should be parsed by motherboard emulation
    allow_port_92_reset = section->Get_bool("allow port 92 reset");

    // CGA/EGA/VGA-specific
    extern unsigned char vga_p3da_undefined_bits;
    vga_p3da_undefined_bits = (unsigned char)section->Get_hex("vga 3da undefined bits");

    // TODO: should be parsed by motherboard emulation or lower level equiv..?
    std::string cmd_machine;
    if (control->cmdline->FindString("-machine",cmd_machine,true)){
        //update value in config (else no matching against suggested values
        section->HandleInputline(std::string("machine=") + cmd_machine);
    }

    // TODO: should be parsed by...? perhaps at some point we support machine= for backwards compat
    //       but translate it into two separate params that specify what machine vs what video hardware.
    //       or better yet as envisioned, a possible dosbox.conf schema that allows a machine with no
    //       base video of it's own, and then to specify an ISA or PCI card attached to the bus that
    //       provides video.
    std::string mtype(section->Get_string("machine"));
    svgaCard = SVGA_None; 
    machine = MCH_VGA;
    int10.vesa_nolfb = false;
    int10.vesa_oldvbe = false;
    if      (mtype == "cga")           { machine = MCH_CGA; mono_cga = false; }
    else if (mtype == "cga_mono")      { machine = MCH_CGA; mono_cga = true; }
    else if (mtype == "cga_rgb")       { machine = MCH_CGA; mono_cga = false; cga_comp = 2; }
    else if (mtype == "cga_composite") { machine = MCH_CGA; mono_cga = false; cga_comp = 1; new_cga = false; }
    else if (mtype == "cga_composite2"){ machine = MCH_CGA; mono_cga = false; cga_comp = 1; new_cga = true; }
    else if (mtype == "mcga")          { machine = MCH_MCGA; }
    else if (mtype == "tandy")         { machine = MCH_TANDY; }
    else if (mtype == "pcjr")          { machine = MCH_PCJR; }
    else if (mtype == "hercules")      { machine = MCH_HERC; }
    else if (mtype == "mda")           { machine = MCH_MDA; }
    else if (mtype == "ega")           { machine = MCH_EGA; }
    else if (mtype == "svga_s3")       { svgaCard = SVGA_S3Trio; }
    else if (mtype == "vesa_nolfb")    { svgaCard = SVGA_S3Trio; int10.vesa_nolfb = true;}
    else if (mtype == "vesa_oldvbe")   { svgaCard = SVGA_S3Trio; int10.vesa_oldvbe = true;}
    else if (mtype == "svga_et4000")   { svgaCard = SVGA_TsengET4K; }
    else if (mtype == "svga_et3000")   { svgaCard = SVGA_TsengET3K; }
    else if (mtype == "svga_paradise") { svgaCard = SVGA_ParadisePVGA1A; }
    else if (mtype == "vgaonly")       { svgaCard = SVGA_None; }
    else if (mtype == "amstrad")       { machine = MCH_AMSTRAD; }
    else if (mtype == "pc98")          { machine = MCH_PC98; }
    else if (mtype == "pc9801")        { machine = MCH_PC98; } /* Future differentiation */
    else if (mtype == "pc9821")        { machine = MCH_PC98; } /* Future differentiation */

    else if (mtype == "fm_towns")      { machine = MCH_FM_TOWNS; }

    else E_Exit("DOSBOX:Unknown machine type %s",mtype.c_str());

    // FM TOWNS is stub!!!
    if (IS_FM_TOWNS) E_Exit("FM Towns emulation not yet implemented");
}

void DOSBOX_SetupConfigSections(void) {
    Prop_int* Pint;
    Prop_hex* Phex;
    Prop_bool* Pbool;
    Prop_string* Pstring;
    Prop_double* Pdouble;
    Prop_multival* Pmulti;
    Section_prop * secprop;
    Prop_multival_remain* Pmulti_remain;

    // Some frequently used option sets
    const char* force[] = { "", "forced", 0 };
    const char* cyclest[] = { "fixed","max","%u",0 };
    const char* blocksizes[] = {"1024", "2048", "4096", "8192", "512", "256", 0};
    const char* controllertypes[] = { "auto", "at", "xt", "pcjr", "pc98", 0}; // Future work: Tandy(?) and USB
    const char* auxdevices[] = {"none","2button","3button","intellimouse","intellimouse45",0};
    const char* cputype_values[] = {"8086", "80186", "286", "386", "486old", "486", "pentium", "pentium_mmx", "ppro_slow", 0};
    const char* rates[] = {  "44100", "48000", "32000","22050", "16000", "11025", "8000", "49716", 0 };
    const char* cpm_compat_modes[] = { "auto", "off", "msdos2", "msdos5", "direct", 0 };
    const char* ems_settings[] = { "true", "emsboard", "emm386", "false", 0};
    const char* truefalseautoopt[] = { "true", "false", "1", "0", "auto", 0};
    const char* pc98videomodeopt[] = { "", "24khz", "31khz", "15khz", 0};
    const char* aspectmodes[] = { "false", "true", "0", "1", "yes", "no", "nearest", "bilinear", 0};

    /* Setup all the different modules making up DOSBox */
    const char* machines[] = {
        "hercules", "cga", "cga_mono", "cga_rgb", "cga_composite", "cga_composite2", "tandy", "pcjr", "ega",
        "vgaonly", "svga_s3", "svga_et3000", "svga_et4000",
        "svga_paradise", "vesa_nolfb", "vesa_oldvbe", "amstrad", "pc98", "pc9801", "pc9821",

        "fm_towns", // STUB

        "mcga", "mda",

        0 };

    const char* scalers[] = { 
        "none", "normal2x", "normal3x", "normal4x", "normal5x",
#if RENDER_USE_ADVANCED_SCALERS>2
        "advmame2x", "advmame3x", "advinterp2x", "advinterp3x", "hq2x", "hq3x", "2xsai", "super2xsai", "supereagle",
#endif
#if RENDER_USE_ADVANCED_SCALERS>0
        "tv2x", "tv3x", "rgb2x", "rgb3x", "scan2x", "scan3x", "gray", "gray2x",
#endif
        "hardware_none", "hardware2x", "hardware3x", "hardware4x", "hardware5x",
        0 };

    const char* cores[] = { "normal", 0 };

#if defined(__SSE__) && !defined(_M_AMD64)
    CheckSSESupport();
#endif
    SDLNetInited = false;

    secprop=control->AddSection_prop("dosbox",&Null_Init);

    Pstring = secprop->Add_string("machine",Property::Changeable::OnlyAtStart,"svga_s3");
    Pstring->Set_values(machines);
    Pstring->Set_help("The type of machine DOSBox tries to emulate.");

    Pint = secprop->Add_int("memsize", Property::Changeable::WhenIdle,16);
    Pint->SetMinMax(1,511);
    Pint->Set_help(
        "Amount of memory DOSBox has in megabytes.\n"
        "  This value is best left at its default to avoid problems with some games,\n"
        "  though few games might require a higher value.\n"
        "  There is generally no speed advantage when raising this value.\n"
        "  Programs that use 286 protected mode like Windows 3.0 in Standard Mode may crash with more than 15MB.");

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
        "    26: 64MB aliasing. Some 486s had only 26 external address bits, some motherboards tied off A26-A31");

    Pbool = secprop->Add_bool("pc-98 BIOS copyright string",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("If set, the PC-98 BIOS copyright string is placed at E800:0000. Enable this for software that detects PC-98 vs Epson.");

    Pbool = secprop->Add_bool("pc-98 int 1b fdc timer wait",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("If set, INT 1Bh floppy access will wait for the timer to count down before returning.\n"
                    "This is needed for Ys II to run without crashing.");

    Pbool = secprop->Add_bool("pc-98 pic init to read isr",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("If set, the programmable interrupt controllers are initialized by default (if PC-98 mode)\n"
                    "so that the in-service interrupt status can be read immediately. There seems to be a common\n"
                    "convention in PC-98 games to program and/or assume this mode for cooperative interrupt handling.\n"
                    "This option is enabled by default for best compatibility with PC-98 games.");

    Pbool = secprop->Add_bool("pc-98 buffer page flip",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("If set, the game's request to page flip will be delayed to vertical retrace, which can eliminate tearline artifacts.\n"
                    "Note that this is NOT the behavior of actual hardware. This option is provided for the user's preference.");

    Pbool = secprop->Add_bool("pc-98 enable 256-color",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Allow 256-color graphics mode if set, disable if not set");

    Pbool = secprop->Add_bool("pc-98 enable 16-color",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Allow 16-color graphics mode if set, disable if not set");

    Pbool = secprop->Add_bool("pc-98 enable grcg",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Allow GRCG graphics functions if set, disable if not set");

    Pbool = secprop->Add_bool("pc-98 enable egc",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Allow EGC graphics functions if set, disable if not set");

    Pbool = secprop->Add_bool("pc-98 enable 188 user cg",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Allow 188+ user-defined CG cells if set");

    Pbool = secprop->Add_bool("pc-98 start gdc at 5mhz",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("Start GDC at 5MHz if set, 2.5MHz if clear. May be required for some games.");

    Pbool = secprop->Add_bool("pc-98 allow scanline effect",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("If set, PC-98 emulation will allow the DOS application to enable the 'scanline effect'\n"
                    "in 200-line graphics modes upconverted to 400-line raster display. When enabled, odd\n"
                    "numbered scanlines are blanked instead of doubled");

    Pbool = secprop->Add_bool("pc-98 bus mouse",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Enable PC-98 bus mouse emulation. Disabling this option does not disable INT 33h emulation.");

    Pstring = secprop->Add_string("pc-98 video mode",Property::Changeable::WhenIdle,"");
    Pstring->Set_values(pc98videomodeopt);
    Pstring->Set_help("Specify the preferred PC-98 video mode.\n"
                      "Valid values are 15, 24, or 31 for each specific horizontal refresh rate on the platform.\n"
                      "24khz is default and best supported at this time.\n"
                      "15khz is not implemented at this time.\n"
                      "31khz is experimental at this time.");

    Pint = secprop->Add_int("pc-98 timer master frequency", Property::Changeable::WhenIdle,0);
    Pint->SetMinMax(0,2457600);
    Pint->Set_help("8254 timer clock frequency (NEC PC-98). Depending on the CPU frequency the clock frequency is one of two common values.\n"
                   "If your setting is neither of the below the closest appropriate value will be chosen.\n"
                   "This setting affects the master clock rate that DOS applications must divide down from to program the timer\n"
                   "at the correct rate, which affects timer interrupt, PC speaker, and the COM1 RS-232C serial port baud rate.\n"
                   "8MHz is treated as an alias for 4MHz and 10MHz is treated as an alias for 5MHz.\n"
                   "    0: Use default (auto)\n"
                   "    4: 1.996MHz (as if 4MHz or multiple thereof CPU clock)\n"
                   "    5: 2.457MHz (as if 5MHz or multiple thereof CPU clock)");

    Pint = secprop->Add_int("pc-98 allow 4 display partition graphics", Property::Changeable::WhenIdle,-1);
    Pint->SetMinMax(-1,1);
    Pint->Set_help("According to NEC graphics controller documentation, graphics mode is supposed to support only\n"
                   "2 display partitions. Some games rely on hardware flaws that allowed 4 partitions.\n"
                   "   -1: Default (choose automatically)\n"
                   "    0: Disable\n"
                   "    1: Enable");

    Pbool = secprop->Add_bool("pc-98 force ibm keyboard layout",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("Force to use a default keyboard layout like IBM US-English for PC-98 emulation.\n"
                    "Will only work with apps and games using BIOS for keyboard.");

    Pint = secprop->Add_int("vga bios size override", Property::Changeable::WhenIdle,0);
    Pint->SetMinMax(512,65536);
    Pint->Set_help("VGA BIOS size override. Override the size of the VGA BIOS (normally 32KB in compatible or 12KB in non-compatible).");

    Pbool = secprop->Add_bool("video bios dont duplicate cga first half rom font",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("If set, save 4KB of EGA/VGA ROM space by pointing to the copy in the ROM BIOS of the first 128 chars");

    Pbool = secprop->Add_bool("video bios always offer 14-pixel high rom font",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("If set, video BIOS will always carry the 14-pixel ROM font. If clear, 14-pixel rom font will not be offered except for EGA/VGA emulation.");

    Pbool = secprop->Add_bool("video bios always offer 16-pixel high rom font",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("If set, video BIOS will always carry the 16-pixel ROM font. If clear, 16-pixel rom font will not be offered except for VGA emulation.");

    Pbool = secprop->Add_bool("video bios enable cga second half rom font",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("If set, and emulating CGA/PCjr/Tandy, automatically provide the second half of the 8x8 ROM font.\n"
            "This setting is ignored for EGA/VGA emulation. If not set, you will need a utility like GRAFTABL.COM to load the second half of the ROM font for graphics.\n"
            "NOTE: if you disable the 14 & 16 pixel high font AND the second half when machine=cga, you will disable video bios completely.");

    Pstring = secprop->Add_string("forcerate",Property::Changeable::Always,"");
    Pstring->Set_help("Force the VGA framerate to a specific value(ntsc, pal, or specific hz), no matter what");

    Pbool = secprop->Add_bool("sierra ramdac",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Whether or not to emulate a Sierra or compatible RAMDAC at port 3C6h-3C9h.\n"
            "Some DOS games expect to access port 3C6h to enable highcolor/truecolor SVGA modes on older chipsets.\n"
            "Disable if you wish to emulate SVGA hardware that lacks a RAMDAC or (depending on the chipset) does\n"
            "not emulate a RAMDAC that is accessible through port 3C6h. This option has no effect for non-VGA video hardware.");

    Pbool = secprop->Add_bool("sierra ramdac lock 565",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("When emulating High Sierra highcolor RAMDAC, assume 5:6:5 at all times if set. Else,\n"
            "bit 6 of the DAC command selects between 5:5:5 and 5:6:5. Set this option for demos or\n"
            "games that got the command byte wrong (MFX Transgrassion 2) or any other demo that is\n"
            "not rendering highcolor 16bpp correctly.");

    Pbool = secprop->Add_bool("page flip debug line",Property::Changeable::Always,false);
    Pbool->Set_help("VGA debugging switch. If set, an inverse line will be drawn on the exact scanline that the CRTC display offset registers were written.\n"
            "This can be used to help diagnose whether or not the DOS game is page flipping properly according to vertical retrace if the display on-screen is flickering.");

    Pbool = secprop->Add_bool("vertical retrace poll debug line",Property::Changeable::Always,false);
    Pbool->Set_help("VGA debugging switch. If set, an inverse green dotted line will be drawn on the exact scanline that the CRTC status port (0x3DA) was read.\n"
            "This can be used to help diagnose whether the DOS game is propertly waiting for vertical retrace.");

    Pbool = secprop->Add_bool("cgasnow",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("When machine=cga, determines whether or not to emulate CGA snow in 80x25 text mode");

    Phex = secprop->Add_hex("vga 3da undefined bits",Property::Changeable::WhenIdle,0);
    Phex->Set_help("VGA status port 3BA/3DAh only defines bits 0 and 3. This setting allows you to assign a bit pattern to the undefined bits.\n"
                   "The purpose of this hack is to deal with demos that read and handle port 3DAh in ways that might crash if all are zero.\n"
                   "By default, this value is zero.");

    Pbool = secprop->Add_bool("unmask timer on int 10 setmode",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("If set, INT 10h will unmask IRQ 0 (timer) when setting video modes.");

    Pbool = secprop->Add_bool("unmask keyboard on int 16 read",Property::Changeable::OnlyAtStart,true);
    Pbool->Set_help("If set, INT 16h will unmask IRQ 1 (keyboard) when asked to read keyboard input.\n"
                    "It is strongly recommended that you set this option if running Windows 3.11 Windows for Workgroups in DOSBox-X.");

    Pbool = secprop->Add_bool("int16 keyboard polling undocumented cf behavior",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("If set, INT 16h function AH=01h will also set/clear the carry flag depending on whether input was available.\n"
                    "There are some old DOS games and demos that rely on this behavior to sense keyboard input, and this behavior\n"
                    "has been verified to occur on some old (early 90s) BIOSes.");

    Pbool = secprop->Add_bool("allow port 92 reset",Property::Changeable::OnlyAtStart,true);
    Pbool->Set_help("If set (default), allow the application to reset the CPU through port 92h");

    Pbool = secprop->Add_bool("enable port 92",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Emulate port 92h (PS/2 system control port A). If you want to emulate a system that pre-dates the PS/2, set to 0.");

    Pbool = secprop->Add_bool("enable 1st dma controller",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Emulate 1st (AT) DMA controller (default). Set to 0 if you wish to emulate a system that lacks DMA (PCjr and some Tandy systems)");

    Pbool = secprop->Add_bool("enable 2nd dma controller",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Emulate 2nd (AT) DMA controller (default). Set to 0 if you wish to emulate a PC/XT system without 16-bit DMA.\n"
            "Note: mainline DOSBox automatically disables 16-bit DMA when machine=cga or machine=hercules, while DOSBox-X does not.");

    Pbool = secprop->Add_bool("allow dma address decrement",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("If set, allow increment & decrement modes as specified in the 8237 datasheet.\n"
            "If clear, always increment the address (as if to emulate clone 8237 implementations that skipped the inc/dec bit).");

    Pstring = secprop->Add_string("enable 128k capable 16-bit dma", Property::Changeable::OnlyAtStart,"auto");
    Pstring->Set_values(truefalseautoopt);
    Pstring->Set_help("If true, DMA controller emulation models ISA hardware that permits 16-bit DMA to span 128KB.\n"
                    "If false, DMA controller emulation models PCI hardware that limits 16-bit DMA to 64KB boundaries.\n"
                    "If auto, the choice is made according to other factors in hardware emulation");

    Pbool = secprop->Add_bool("enable dma extra page registers",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("If set, emulate the extra page registers (I/O ports 0x80, 0x84-0x86, 0x88, 0x8C-0x8E), like actual hardware.\n"
            "Note that mainline DOSBox behavior is to NOT emulate these registers.");

    Pbool = secprop->Add_bool("dma page registers write-only",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("Normally (on AT hardware) the DMA page registers are read/write. Set this option if you want to emulate PC/XT hardware where the page registers are write-only.");

    Pbool = secprop->Add_bool("cascade interrupt never in service",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("If set, PIC emulation will never mark cascade interrupt as in service. This is OFF by default. It is a hack for troublesome games.");

    Pbool = secprop->Add_bool("cascade interrupt ignore in service",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("If set, PIC emulation will allow slave pic interrupts even if the cascade interrupt is still \"in service\". This is OFF by default. It is a hack for troublesome games.");

    Pbool = secprop->Add_bool("enable slave pic",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Enable slave PIC (IRQ 8-15). Set this to 0 if you want to emulate a PC/XT type arrangement with IRQ 0-7 and no IRQ 2 cascade.");

    Pbool = secprop->Add_bool("enable pc nmi mask",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Enable PC/XT style NMI mask register (0xA0). Note that this option conflicts with the secondary PIC and will be ignored if the slave PIC is enabled.");

    Pbool = secprop->Add_bool("rom bios 8x8 CGA font",Property::Changeable::Always,true);
    Pbool->Set_help("If set, or mainline compatible bios mapping, a legacy 8x8 CGA font (first 128 characters) is stored at 0xF000:0xFA6E. DOS programs that do not use INT 10h to locate fonts might require that font to be located there.");

    Pbool = secprop->Add_bool("rom bios video parameter table",Property::Changeable::Always,true);
    Pbool->Set_help("If set, or mainline compatible bios mapping, DOSBox will emulate the video parameter table and assign that to INT 1Dh. If clear, table will not be provided.");

    Pbool = secprop->Add_bool("allow more than 640kb base memory",Property::Changeable::Always,false);
    Pbool->Set_help("If set, and space is available, allow conventional memory to extend past 640KB.\n"
            "For example, if machine=cga, conventional memory can extend out to 0xB800 and provide up to 736KB of RAM.\n"
            "This allows you to emulate PC/XT style memory extensions.");

    /* should be set to zero unless for very specific demos:
     *  - "Melvindale" by MFX (1996): Set this to 2, the nightmarish visual rendering code appears to draw 2 scanlines
     *    upward from the VESA linear framebuffer base we return, causing DOSBox to emit warnings about illegal read/writes
     *    from 0xBFFFF000-0xBFFFFFFF (just before the base of the framebuffer at 0xC0000000). It also has the effect of
     *    properly centering the picture on the screen. I suppose it's a miracle the demo didn't crash people's computers
     *    writing to undefined areas like that. */
    Pint = secprop->Add_int("vesa lfb base scanline adjust",Property::Changeable::WhenIdle,0);
    Pint->Set_help("If non-zero, the VESA BIOS will report the linear framebuffer offset by this many scanlines.\n"
            "This does not affect the linear framebuffer's location. It only affects the linear framebuffer\n"
            "location reported by the VESA BIOS. Set to nonzero for DOS games with sloppy VESA graphics pointer management.\n"
            "    MFX \"Melvindale\" (1996): Set this option to 2 to center the picture properly.");

    /* If set, all VESA BIOS modes map 128KB of video RAM at A0000-BFFFF even though VESA BIOS emulation
     * reports a 64KB window. Some demos like the 1996 Wired report
     * (ftp.scene.org/pub/parties/1995/wired95/misc/e-w95rep.zip) assume they can write past the window
     * by spilling into B0000 without bank switching. */
    Pbool = secprop->Add_bool("vesa map non-lfb modes to 128kb region",Property::Changeable::Always,false);
    Pbool->Set_help("If set, VESA BIOS SVGA modes will be set to map 128KB of video memory to A0000-BFFFF instead of\n"
                    "64KB at A0000-AFFFF. This does not affect the SVGA window size or granularity.\n"
                    "Some games or demoscene productions assume that they can render into the next SVGA window/bank\n"
                    "by writing to video memory beyond the current SVGA window address and will not appear correctly\n"
                    "without this option.");

    Pbool = secprop->Add_bool("allow hpel effects",Property::Changeable::Always,false);
    Pbool->Set_help("If set, allow the DOS demo or program to change the horizontal pel (panning) register per scanline.\n"
            "Some early DOS demos use this to create waving or sinus effects on the picture. Not very many VGA\n"
            "chipsets allow this, so far, only ATI chipsets are known to support this effect. Disabled by default.");

    Pbool = secprop->Add_bool("allow hretrace effects",Property::Changeable::Always,false);
    Pbool->Set_help("If set, allow the DOS demo or program to make the picture wavy by playing with the 'start horizontal"
            "retrace' register of the CRTC during the active picture. Some early DOS demos (Copper by Surprise!"
            "productions) need this option set for some demo effects to work. Disabled by default.");

    Pdouble = secprop->Add_double("hretrace effect weight",Property::Changeable::Always,4.0);
    Pdouble->Set_help("If emulating hretrace effects, this parameter adds 'weight' to the offset to smooth it out.\n"
            "the larger the number, the more averaging is applied. This is intended to emulate the inertia\n"
            "of the electron beam in a CRT monitor");

    Pint = secprop->Add_int("vesa modelist cap",Property::Changeable::Always,0);
    Pint->Set_help("IF nonzero, the VESA modelist is capped so that it contains no more than the specified number of video modes.\n"
            "Set this option to a value between 8 to 32 if the DOS application has problems with long modelists or a fixed\n"
            "buffer for querying modes. Such programs may crash if given the entire modelist supported by DOSBox-X.\n"
            "  Warcraft II by Blizzard ................ Set to a value between 8 and 16. This game has a fixed buffer that it\n"
            "                                           reads the modelist into. DOSBox-X's normal modelist is too long and\n"
            "                                           the game will overrun the buffer and crash without this setting.");

    Pint = secprop->Add_int("vesa modelist width limit",Property::Changeable::Always,0);
    Pint->Set_help("IF nonzero, VESA modes with horizontal resolution higher than the specified pixel count will not be listed.\n"
            "This is another way the modelist can be capped for DOS applications that have trouble with long modelists.");

    Pint = secprop->Add_int("vesa modelist height limit",Property::Changeable::Always,0);
    Pint->Set_help("IF nonzero, VESA modes with vertical resolution higher than the specified pixel count will not be listed.\n"
            "This is another way the modelist can be capped for DOS applications that have trouble with long modelists.");

    Pbool = secprop->Add_bool("vesa vbe put modelist in vesa information",Property::Changeable::Always,false);
    Pbool->Set_help("If set, the VESA modelist is placed in the VESA information structure itself when the DOS application\n"
                    "queries information on the VESA BIOS. Setting this option may help with some games, though it limits\n"
                    "the mode list reported to the DOS application.");

    Pbool = secprop->Add_bool("vesa vbe 1.2 modes are 32bpp",Property::Changeable::Always,true);
    Pbool->Set_help("If set, truecolor (16M color) VESA BIOS modes in the 0x100-0x11F range are 32bpp. If clear, they are 24bpp.\n"
            "Some DOS games and demos assume one bit depth or the other and do not enumerate VESA BIOS modes, which is why this\n"
            "option exists.");

    Pbool = secprop->Add_bool("allow low resolution vesa modes",Property::Changeable::Always,true);
    Pbool->Set_help("If set, allow low resolution VESA modes (320x200x16/24/32bpp and so on). You could set this to false to simulate\n"
            "SVGA hardware with a BIOS that does not support the lowres modes for testing purposes.");

    Pbool = secprop->Add_bool("allow 32bpp vesa modes",Property::Changeable::Always,true);
    Pbool->Set_help("If the DOS game or demo has problems with 32bpp VESA modes, set to 'false'");

    Pbool = secprop->Add_bool("allow 24bpp vesa modes",Property::Changeable::Always,true);
    Pbool->Set_help("If the DOS game or demo has problems with 24bpp VESA modes, set to 'false'");

    Pbool = secprop->Add_bool("allow 16bpp vesa modes",Property::Changeable::Always,true);
    Pbool->Set_help("If the DOS game or demo has problems with 16bpp VESA modes, set to 'false'");

    Pbool = secprop->Add_bool("allow 15bpp vesa modes",Property::Changeable::Always,true);
    Pbool->Set_help("If the DOS game or demo has problems with 15bpp VESA modes, set to 'false'");

    Pbool = secprop->Add_bool("allow 8bpp vesa modes",Property::Changeable::Always,true);
    Pbool->Set_help("If the DOS game or demo has problems with 8bpp VESA modes, set to 'false'");

    Pbool = secprop->Add_bool("allow 4bpp vesa modes",Property::Changeable::Always,true);
    Pbool->Set_help("If the DOS game or demo has problems with 4bpp VESA modes, set to 'false'");

    Pbool = secprop->Add_bool("allow 4bpp packed vesa modes",Property::Changeable::Always,true);
    Pbool->Set_help("If the DOS game or demo has problems with 4bpp packed VESA modes, set to 'false'");

    Pbool = secprop->Add_bool("allow tty vesa modes",Property::Changeable::Always,true);
    Pbool->Set_help("If the DOS game or demo has problems with text VESA modes, set to 'false'");

    secprop=control->AddSection_prop("render",&Null_Init,true);
    Pint = secprop->Add_int("frameskip",Property::Changeable::Always,0);
    Pint->SetMinMax(0,10);
    Pint->Set_help("How many frames DOSBox skips before drawing one.");

    Pbool = secprop->Add_bool("alt render",Property::Changeable::Always,false);
    Pbool->Set_help("If set, use a new experimental rendering engine");

    Pstring = secprop->Add_string("aspect", Property::Changeable::Always, "false");
    Pstring->Set_values(aspectmodes);
    Pstring->Set_help(
        "Aspect ratio correction mode. Can be set to the following values:\n"
        "  'false' (default):\n"
        "      'direct3d'/opengl outputs: image is simply scaled to full window/fullscreen size, possibly resulting in disproportional image\n"
        "      'surface' output: it does no aspect ratio correction (default), resulting in disproportional images if VGA mode pixel ratio is not 4:3\n"
        "  'true':\n"
        "      'direct3d'/opengl outputs: uses output driver functions to scale / pad image with black bars, correcting output to proportional 4:3 image\n"
        "          In most cases image degradation should not be noticeable (it all depends on the video adapter and how much the image is upscaled).\n"
        "          Should have none to negligible impact on performance, mostly being done in hardware\n"
        "      'surface' output: inherits old DOSBox aspect ratio correction method (adjusting rendered image line count to correct output to 4:3 ratio)\n"
        "          Due to source image manipulation this mode does not mix well with scalers, i.e. multiline scalers like hq2x/hq3x will work poorly\n"
        "          Slightly degrades visual image quality. Has a tiny impact on performance"
#if C_SURFACE_POSTRENDER_ASPECT
        "\n"
        "  'nearest':\n"
        "      'direct3d'/opengl outputs: not available, fallbacks to 'true' mode automatically\n"
        "      'surface' output: scaler friendly aspect ratio correction, works by rescaling rendered image using nearest neighbor scaler\n"
        "          Complex scalers work. Image quality is on par with 'true' mode (and better with scalers). More CPU intensive than 'true' mode\n"
        "  'bilinear':\n"
        "      'direct3d'/opengl outputs: not available, fallbacks to 'true' mode automatically\n"
        "      'surface' output: scaler friendly aspect ratio correction, works by rescaling rendered image using bilinear scaler\n"
        "          Complex scalers work. Image quality is much better, should be on par with using 'direct3d' output + 'true' mode\n"
        "          Very CPU intensive, high end CPU may be required"
#endif
    );

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
    Pmulti->SetValue("normal2x",/*init*/true);
    Pmulti->Set_help("Scaler used to enlarge/enhance low resolution modes. If 'forced' is appended,\n"
                     "then the scaler will be used even if the result might not be desired.");
    Pstring = Pmulti->GetSection()->Add_string("type",Property::Changeable::Always,"normal2x");
    Pstring->Set_values(scalers);

    Pstring = Pmulti->GetSection()->Add_string("force",Property::Changeable::Always,"");
    Pstring->Set_values(force);

    Pbool = secprop->Add_bool("autofit",Property::Changeable::Always,true);
    Pbool->Set_help(
        "Best fits image to window\n"
        "- Intended for output=direct3d, fullresolution=original, aspect=true");

    Pmulti = secprop->Add_multi("monochrome_pal",Property::Changeable::Always," ");
    Pmulti->SetValue("green",/*init*/true);
    Pmulti->Set_help("Specify the color of monochrome display.\n"
        "Possible values: green, amber, gray, white\n"
        "Append 'bright' for a brighter look.");
    Pstring = Pmulti->GetSection()->Add_string("color",Property::Changeable::Always,"green");
    const char* monochrome_pal_colors[]={
      "green","amber","gray","white",0
    };
    Pstring->Set_values(monochrome_pal_colors);
    Pstring = Pmulti->GetSection()->Add_string("bright",Property::Changeable::Always,"");
    const char* bright[] = { "", "bright", 0 };
    Pstring->Set_values(bright);

    secprop=control->AddSection_prop("cpu",&Null_Init,true);//done
    Pstring = secprop->Add_string("core",Property::Changeable::WhenIdle,"normal");
    Pstring->Set_values(cores);
    Pstring->Set_help("CPU Core used in emulation. auto will switch to dynamic if available and appropriate.\n"
            "WARNING: Do not use dynamic or auto setting core with Windows 95 or other preemptive\n"
            "multitasking OSes with protected mode paging, you should use the normal core instead.");

    Pbool = secprop->Add_bool("fpu",Property::Changeable::Always,true);
    Pbool->Set_help("Enable FPU emulation");

    Pbool = secprop->Add_bool("always report double fault",Property::Changeable::Always,false);
    Pbool->Set_help("Always report (to log file) double faults if set. Else, a double fault is reported only once. Set this option for debugging purposes.");

    Pbool = secprop->Add_bool("always report triple fault",Property::Changeable::Always,false);
    Pbool->Set_help("Always report (to log file) triple faults if set. Else, a triple fault is reported only once. Set this option for debugging purposes.");

    Pbool = secprop->Add_bool("enable msr",Property::Changeable::Always,true);
    Pbool->Set_help("Allow RDMSR/WRMSR instructions. This option is only meaningful when cputype=pentium.\n"
            "WARNING: Leaving this option enabled while installing Windows 95/98/ME can cause crashes.");

    Pbool = secprop->Add_bool("enable cmpxchg8b",Property::Changeable::Always,true);
    Pbool->Set_help("Enable Pentium CMPXCHG8B instruction. Enable this explicitly if using software that uses this instruction.\n"
            "You must enable this option to run Windows ME because portions of the kernel rely on this instruction.");

    Pbool = secprop->Add_bool("ignore undefined msr",Property::Changeable::Always,false);
    Pbool->Set_help("Ignore RDMSR/WRMSR on undefined registers. Normally the CPU will fire an Invalid Opcode exception in that case.\n"
            "This option is off by default, enable if using software or drivers that assumes the presence of\n"
            "certain MSR registers without checking. If you are using certain versions of the 3Dfx glide drivers for MS-DOS\n"
            "you will need to set this to TRUE as 3Dfx appears to have coded GLIDE2.OVL to assume the presence\n"
            "of Pentium Pro/Pentium II MTRR registers.\n"
            "WARNING: Leaving this option enabled while installing Windows 95/98/ME can cause crashes.");

    Pint = secprop->Add_int("interruptible rep string op",Property::Changeable::Always,-1);
    Pint->SetMinMax(-1,65536);
    Pint->Set_help("if nonzero, REP string instructions (LODS/MOVS/STOS/INS/OUTS) are interruptible (by interrupts or other events).\n"
            "if zero, REP string instructions are carried out in full before processing events and interrupts.\n"
            "Set to -1 for a reasonable default setting based on cpu type and other configuration.\n"
            "A setting of 0 can improve emulation speed at the expense of emulation accuracy.\n"
            "A nonzero setting (1-8) may be needed for DOS games and demos that use the IRQ 0 interrupt to play digitized samples\n"
            "while doing VGA palette animation at the same time (use case of REP OUTS), where the non-interruptible version\n"
            "would cause an audible drop in audio pitch.");

    Pstring = secprop->Add_string("cputype",Property::Changeable::Always,"pentium");
    Pstring->Set_values(cputype_values);
    Pstring->Set_help("CPU Type used in emulation");

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

    Pstring = Pmulti_remain->GetSection()->Add_string("type",Property::Changeable::Always,"fixed");
    Pmulti_remain->SetValue("fixed",/*init*/true);
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

    // CLEANUP TODO: "Integration device" does not belong in CPU section!
    Pbool = secprop->Add_bool("integration device",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("Enable DOSBox integration I/O device. This can be used by the guest OS to match mouse pointer position, for example. EXPERIMENTAL!");

    secprop=control->AddSection_prop("keyboard",&Null_Init);
    Pbool = secprop->Add_bool("aux",Property::Changeable::OnlyAtStart,true);
    Pbool->Set_help("Enable emulation of the 8042 auxiliary port. PS/2 mouse emulation requires this to be enabled.\n"
            "You should enable this if you will be running Windows ME or any other OS that does not use the BIOS to receive mouse events.");

    Pbool = secprop->Add_bool("allow output port reset",Property::Changeable::OnlyAtStart,true);
    Pbool->Set_help("If set (default), allow the application to reset the CPU through the keyboard controller.\n"
            "This option is required to allow Windows ME to reboot properly, whereas Windows 9x and earlier\n"
            "will reboot without this option using INT 19h");

    Pstring = secprop->Add_string("controllertype",Property::Changeable::OnlyAtStart,"auto");
    Pstring->Set_values(controllertypes);
    Pstring->Set_help("Type of keyboard controller (and keyboard) attached.\n"
                      "auto     Automatically pick according to machine type\n"
                      "at       AT (PS/2) type keyboard\n"
                      "xt       IBM PC/XT type keyboard\n"
                      "pcjr     IBM PCjr type keyboard (only if machine=pcjr)\n"
                      "pc98     PC-98 keyboard emulation (only if machine=pc98)");

    Pstring = secprop->Add_string("auxdevice",Property::Changeable::OnlyAtStart,"intellimouse");
    Pstring->Set_values(auxdevices);
    Pstring->Set_help("Type of PS/2 mouse attached to the AUX port");

    secprop=control->AddSection_prop("mixer",&Null_Init);
    Pbool = secprop->Add_bool("nosound",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("Enable silent mode, sound is still emulated though.");

    Pbool = secprop->Add_bool("sample accurate",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("Enable sample accurate mixing, at the expense of some emulation performance. Enable this option for DOS games and demos that\n"
            "require such accuracy for correct Tandy/OPL output including digitized speech. This option can also help eliminate minor\n"
            "errors in Gravis Ultrasound emulation that result in random echo/attenuation effects.");

    Pbool = secprop->Add_bool("swapstereo",Property::Changeable::OnlyAtStart,false); 
    Pbool->Set_help("Swaps the left and right stereo channels."); 

    Pint = secprop->Add_int("rate",Property::Changeable::OnlyAtStart,44100);
    Pint->Set_values(rates);
    Pint->Set_help("Mixer sample rate, setting any device's rate higher than this will probably lower their sound quality.");

    Pint = secprop->Add_int("blocksize",Property::Changeable::OnlyAtStart,1024);
    Pint->Set_values(blocksizes);
    Pint->Set_help("Mixer block size, larger blocks might help sound stuttering but sound will also be more lagged.");

    Pint = secprop->Add_int("prebuffer",Property::Changeable::OnlyAtStart,20);
    Pint->SetMinMax(0,100);
    Pint->Set_help("How many milliseconds of data to keep on top of the blocksize.");

    /* All the DOS Related stuff, which will eventually start up in the shell */
    secprop=control->AddSection_prop("dos",&Null_Init,false);//done
    Pbool = secprop->Add_bool("xms",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Enable XMS support.");

    Pint = secprop->Add_int("xms handles",Property::Changeable::WhenIdle,0);
    Pint->Set_help("Number of XMS handles available for the DOS environment, or 0 to use a reasonable default");

    Pbool = secprop->Add_bool("shell configuration as commands",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("Allow entering dosbox.conf configuration parameters as shell commands to get and set settings.\n"
                    "This is disabled by default to avoid conflicts between commands and executables.\n"
                    "It is recommended to get and set dosbox.conf settings using the CONFIG command instead.\n"
                    "Compatibility with DOSBox SVN can be improved by enabling this option.");

    Pbool = secprop->Add_bool("hma",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Report through XMS that HMA exists (not necessarily available)");

    Pbool = secprop->Add_bool("hma allow reservation",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Allow TSR and application (anything other than the DOS kernel) to request control of the HMA.\n"
            "They will not be able to request control however if the DOS kernel is configured to occupy the HMA (DOS=HIGH)");

    Pint = secprop->Add_int("hard drive data rate limit",Property::Changeable::WhenIdle,-1);
    Pint->Set_help("Slow down (limit) hard disk throughput. This setting controls the limit in bytes/second.\n"
                   "Set to 0 to disable the limit, or -1 to use a reasonable default.");

    Pint = secprop->Add_int("hma minimum allocation",Property::Changeable::WhenIdle,0);
    Pint->Set_help("Minimum allocation size for HMA in bytes (equivalent to /HMAMIN= parameter).");

    Pbool = secprop->Add_bool("log console",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("If set, log DOS CON output to the log file.");

    Pbool = secprop->Add_bool("dos in hma",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Report that DOS occupies HMA (equiv. DOS=HIGH)");

    Pint = secprop->Add_int("dos sda size",Property::Changeable::WhenIdle,0);
    Pint->Set_help("SDA (swappable data area) size, in bytes. Set to 0 to use a reasonable default.");

    Pint = secprop->Add_int("hma free space",Property::Changeable::WhenIdle,34*1024); /* default 34KB (TODO: How much does MS-DOS 5.0 usually occupy?) */
    Pint->Set_help("Controls the amount of free space available in HMA. This setting is not meaningful unless the\n"
            "DOS kernel occupies HMA and the emulated DOS version is at least 5.0.");

    Pstring = secprop->Add_string("cpm compatibility mode",Property::Changeable::WhenIdle,"auto");
    Pstring->Set_values(cpm_compat_modes);
    Pstring->Set_help(
            "This controls how the DOS kernel sets up the CP/M compatibility code in the PSP segment.\n"
            "Several options are provided to emulate one of several undocumented behaviors related to the CP/M entry point.\n"
            "If set to auto, DOSBox-X will pick the best option to allow it to work properly.\n"
            "Unless set to 'off', this option will require the DOS kernel to occupy the first 256 bytes of the HMA memory area\n"
            "to prevent crashes when the A20 gate is switched on.\n"
            "   auto      Pick the best option\n"
            "   off       Turn off the CP/M entry point (program will abort if called)\n"
            "   msdos2    MS-DOS 2.x behavior, offset field also doubles as data segment size\n"
            "   msdos5    MS-DOS 5.x behavior, entry point becomes one of two fixed addresses\n"
            "   direct    Non-standard behavior, encode the CALL FAR directly to the entry point rather than indirectly");

    Pbool = secprop->Add_bool("share",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Report SHARE.EXE as resident. Does not actually emulate SHARE functions.");

    // bugfix for "Digital Dream" DOS demo that displays a "peace to hackers" message if it thinks it's being debugged.
    Pbool = secprop->Add_bool("write plain iretf for debug interrupts",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("If true (default), the DOS kernel will create an alternate interrupt handler for debug interrupts INT 1 and INT 3\n"
            "that contain ONLY an IRETF instruction. If false, INT 1 and INT 3 will use the same default interrupt handler in\n"
            "the DOS kernel, which contains a callback instruction followed by IRETF. Some DOS games/demos assume they are being\n"
            "debugged if the debug interrupts point to anything other than an IRETF instruction. Set this option to false if\n"
            "you need notification that INT 1/INT 3 was not handled.");

    Phex = secprop->Add_hex("minimum dos initial private segment", Property::Changeable::WhenIdle,0);
    Phex->Set_help("In non-mainline mapping mode, where DOS structures are allocated from base memory, this sets the\n"
            "minimum segment value. Recommended value is 0x70. You may reduce the value down to 0x50 if freeing\n"
            "up more memory is important. Set to 0 for default.");

    Phex = secprop->Add_hex("minimum mcb segment", Property::Changeable::WhenIdle,0);
    Phex->Set_help("Minimum segment value to begin memory allocation from, in hexadecimal. Set to 0 for default.\n"
            "You can increase available DOS memory by reducing this value down to as low as 0x51, however\n"
            "setting it to low can cause some DOS programs to crash or run erratically, and some DOS games\n"
            "and demos to cause intermittent static noises when using Sound Blaster output. DOS programs\n"
            "compressed with Microsoft EXEPACK will not run if the minimum MCB segment is below 64KB.");

    Phex = secprop->Add_hex("minimum mcb free", Property::Changeable::WhenIdle,0);
    Phex->Set_help("Minimum free segment value to leave free. At startup, the DOS kernel will allocate memory\n"
                   "up to this point. This can be used to deal with EXEPACK issues or DOS programs that cannot\n"
                   "be loaded too low in memory. This differs from 'minimum mcb segment' in that this affects\n"
                   "the lowest free block instead of the starting point of the mcb chain.");

    // TODO: Enable by default WHEN the 'SD' signature becomes valid, and a valid device list within
    //       is emulated properly.
    Pbool = secprop->Add_bool("enable dummy device mcb",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("If set (default), allocate a fake device MCB at the base of conventional memory.\n"
            "Clearing this option can reclaim a small amount of conventional memory at the expense of\n"
            "some minor DOS compatibility.");

    Pint = secprop->Add_int("maximum environment block size on exec", Property::Changeable::WhenIdle,-1);
    Pint->SetMinMax(-1,65535);
    Pint->Set_help("Maximum environment block size to copy for child processes. Set to -1 for default.");

    Pint = secprop->Add_int("additional environment block size on exec", Property::Changeable::WhenIdle,-1);
    Pint->SetMinMax(-1,65535);
    Pint->Set_help("When executing a program, compute the size of the parent block then add this amount to allow for a few additional variables.\n"
            "If the subprocesses will never add/modify the environment block, you can free up a few additional bytes by setting this to 0.\n"
            "Set to -1 for default setting.");

    // DEPRECATED, REMOVE
    Pbool = secprop->Add_bool("enable a20 on windows init",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("If set, DOSBox will enable the A20 gate when Windows 3.1/9x broadcasts the INIT message\n"
            "at startup. Windows 3.1 appears to make assumptions at some key points on startup about\n"
            "A20 that don't quite hold up and cause Windows 3.1 to crash when you set A20 emulation\n"
            "to a20=mask as opposed to a20=fast. This option is enabled by default.");

    Pbool = secprop->Add_bool("zero memory on xms memory allocation",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("If set, memory returned by XMS allocation call is zeroed first. This is NOT what\n"
            "DOS actually does, but if set, can help certain DOS games and demos cope with problems\n"
            "related to uninitialized variables in extended memory. When enabled this option may\n"
            "incur a slight to moderate performance penalty.");

    Pstring = secprop->Add_string("ems",Property::Changeable::WhenIdle,"true");
    Pstring->Set_values(ems_settings);
    Pstring->Set_help("Enable EMS support. The default (=true) provides the best\n"
        "compatibility but certain applications may run better with\n"
        "other choices, or require EMS support to be disabled (=false)\n"
        "to work at all.");

    Pbool = secprop->Add_bool("vcpi",Property::Changeable::OnlyAtStart,true);
    Pbool->Set_help("If set and expanded memory is enabled, also emulate VCPI.");

    Pbool = secprop->Add_bool("unmask timer on disk io",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("If set, INT 21h emulation will unmask IRQ 0 (timer interrupt) when the application opens/closes/reads/writes files.");

    Pbool = secprop->Add_bool("zero int 67h if no ems",Property::Changeable::OnlyAtStart,true);
    Pbool->Set_help("If ems=false, leave interrupt vector 67h zeroed out (default true).\n"
            "This is a workaround for games or demos that try to detect EMS by whether or not INT 67h is 0000:0000 rather than a proper test.\n"
            "This option also affects whether INT 67h is zeroed when booting a guest OS");

    /* FIXME: The vm86 monitor in src/ints/ems.cpp is not very stable! Option is default OFF until stabilized! */
    Pbool = secprop->Add_bool("emm386 startup active",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("If set and expanded memory is set to emulate emm386, start the DOS machine with EMM386.EXE active\n"
            "(running the 16-bit DOS environment from within Virtual 8086 mode). If you will be running anything\n"
            "that involves a DOS extender you will also need to enable the VCPI interface as well.");

    Pbool = secprop->Add_bool("zero memory on ems memory allocation",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("If set, memory returned by EMS allocation call is zeroed first. This is NOT what\n"
            "DOS actually does, but if set, can help certain DOS games and demos cope with problems\n"
            "related to uninitialized variables in expanded memory. When enabled this option may\n"
            "incur a slight to moderate performance penalty.");

    Pint = secprop->Add_int("ems system handle memory size",Property::Changeable::WhenIdle,384);
    Pint->Set_help("Amount of memory associated with system handle, in KB");

    Pbool = secprop->Add_bool("ems system handle on even megabyte",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("If set, try to allocate the EMM system handle on an even megabyte.\n"
            "If the DOS game or demo fiddles with the A20 gate while using EMM386.EXE emulation in virtual 8086 mode, setting this option may help prevent crashes.\n"
            "However, forcing allocation on an even megabyte will also cause some extended memory fragmentation and reduce the\n"
            "overall amount of extended memory available to the DOS game depending on whether it expects large contiguous chunks\n"
            "of extended memory.");

    Pbool = secprop->Add_bool("umb",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Enable UMB support.");

    Phex = secprop->Add_hex("umb start",Property::Changeable::OnlyAtStart,0); /* <- (0=auto) 0xD000 is mainline DOSBox compatible behavior */
    Phex->Set_help("UMB region starting segment");

    Phex = secprop->Add_hex("umb end",Property::Changeable::OnlyAtStart,0); /* <- (0=auto) 0xEFFF is mainline DOSBox compatible (where base=0xD000 and size=0x2000) */
    Phex->Set_help("UMB region last segment");

    Pbool = secprop->Add_bool("kernel allocation in umb",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("If set, dynamic kernel allocation=1, and private area in umb=1, all kernel structures will be allocated from the private area in UMB.\n"
            "If you intend to run Windows 3.1 in DOSBox, you must set this option to false else Windows 3.1 will not start.");

    Pbool = secprop->Add_bool("keep umb on boot",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("If emulating UMBs, keep the UMB around after boot (Mainline DOSBox behavior). If clear, UMB is unmapped when you boot an operating system.");

    Pbool = secprop->Add_bool("keep private area on boot",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("If set, keep the DOSBox private area around after boot (Mainline DOSBox behavior). If clear, unmap and discard the private area when you boot an operating system.");

    Pbool = secprop->Add_bool("private area in umb",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("If set, keep private DOS segment in upper memory block, usually segment 0xC800 (Mainline DOSBox behavior)\n"
            "If clear, place private DOS segment at the base of system memory (just below the MCB)");

    Pstring = secprop->Add_string("ver",Property::Changeable::WhenIdle,"");
    Pstring->Set_help("Set DOS version. Specify as major.minor format. A single number is treated as the major version (LFN patch compat). Common settings are:\n"
            "auto (or unset)                  Pick a DOS kernel version automatically\n"
            "3.3                              MS-DOS 3.3 emulation (not tested!)\n"
            "5.0                              MS-DOS 5.0 emulation (recommended for DOS gaming)\n"
            "6.22                             MS-DOS 6.22 emulation\n"
            "7.0                              Windows 95 (pure DOS mode) emulation\n");

    Pbool = secprop->Add_bool("automount",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Enable automatic mount.");

    Pbool = secprop->Add_bool("int33",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Enable INT 33H (mouse) support.");

    Pbool = secprop->Add_bool("int33 hide host cursor if interrupt subroutine",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("If set, the cursor on the host will be hidden if the DOS application provides it's own\n"
                    "interrupt subroutine for the mouse driver to call, which is usually an indication that\n"
                    "the DOS game wishes to draw the cursor with it's own support routines (DeluxePaint II).");

    Pbool = secprop->Add_bool("int33 hide host cursor when polling",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("If set, the cursor on the host will be hidden even if the DOS application has also\n"
                    "hidden the cursor in the guest, as long as the DOS application is polling position\n"
                    "and button status. This can be useful for DOS programs that draw the cursor on their\n"
                    "own instead of using the mouse driver, including most games and DeluxePaint II.");

    Pbool = secprop->Add_bool("int33 disable cell granularity",Property::Changeable::WhenIdle,false);
    Pbool->Set_help("If set, the mouse pointer position is reported at full precision (as if 640x200 coordinates) in all modes.\n"
                    "If not set, the mouse pointer position is rounded to the top-left corner of a character cell in text modes.\n"
                    "This option is OFF by default.");

    Pbool = secprop->Add_bool("int 13 extensions",Property::Changeable::WhenIdle,true);
    Pbool->Set_help("Enable INT 13h extensions (functions 0x40-0x48). You will need this enabled if the virtual hard drive image is 8.4GB or larger.");

    Pbool = secprop->Add_bool("biosps2",Property::Changeable::OnlyAtStart,true);
    Pbool->Set_help("Emulate BIOS INT 15h PS/2 mouse services\n"
        "Note that some OS's like Microsoft Windows neither use INT 33h nor\n"
        "probe the AUX port directly and depend on this BIOS interface exclusively\n"
        "for PS/2 mouse support. In other cases there is no harm in leaving this enabled");

    /* bugfix for Yodel "mayday" demo */
    /* TODO: Set this option to default to "true" if it turns out most BIOSes unmask the IRQ during INT 15h AH=86 WAIT */
    Pbool = secprop->Add_bool("int15 wait force unmask irq",Property::Changeable::OnlyAtStart,true);
    Pbool->Set_help("Some demos or games mistakingly use INT 15h AH=0x86 (WAIT) while leaving the IRQs needed for it masked.\n"
            "If this option is set (by default), the necessary IRQs will be unmasked when INT 15 AH=0x86 is used so that the game or demo does not hang.");

    Pbool = secprop->Add_bool("int15 mouse callback does not preserve registers",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("Set to true if the guest OS or DOS program assigns an INT 15h mouse callback,\n"
            "but does not properly preserve CPU registers. Diagnostic function only (default off).");

    Pstring = secprop->Add_string("keyboardlayout",Property::Changeable::WhenIdle, "auto");
    Pstring->Set_help("Language code of the keyboard layout (or none).");

    Pbool = secprop->Add_bool("dbcs",Property::Changeable::OnlyAtStart,true);
    Pbool->Set_help("Enable DBCS table.\n"
            "CAUTION: Some software will crash without the DBCS table, including the Open Watcom installer.\n");

    Pbool = secprop->Add_bool("filenamechar",Property::Changeable::OnlyAtStart,true);
    Pbool->Set_help("Enable filename char table");

    Pbool = secprop->Add_bool("collating and uppercase",Property::Changeable::OnlyAtStart,true);
    Pbool->Set_help("Enable collating and uppercase table");

    Pint = secprop->Add_int("files",Property::Changeable::OnlyAtStart,127);
    Pint->Set_help("Number of file handles available to DOS programs. (equivalent to \"files=\" in config.sys)");

    // DEPRECATED, REMOVE
    Pbool = secprop->Add_bool("con device use int 16h to detect keyboard input",Property::Changeable::OnlyAtStart,true);
    Pbool->Set_help("If set, use INT 16h to detect keyboard input (MS-DOS 6.22 behavior). If clear, detect keyboard input by\n"
            "peeking into the BIOS keyboard buffer (Mainline DOSBox behavior). You will need to set this\n"
            "option for programs that hook INT 16h to handle keyboard input ahead of the DOS console.\n"
            "Microsoft Scandisk needs this option to respond to keyboard input correctly.");

    Pbool = secprop->Add_bool("zero memory on int 21h memory allocation",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("If set, memory returned by the INT 21h allocation call is zeroed first. This is NOT what\n"
            "DOS actually does, but if set, can help certain DOS games and demos cope with problems\n"
            "related to uninitialized variables in the data or stack segment. If you intend to run a\n"
            "game or demo known to have this problem (Second Unreal, for example), set to true, else\n"
            "set to false. When enabled this option may incur a slight to moderate performance penalty.");

    //TODO ?
    control->AddSection_line("autoexec",&Null_Init);
    MSG_Add("AUTOEXEC_CONFIGFILE_HELP",
        "Lines in this section will be run at startup.\n"
        "You can put your MOUNT lines here.\n"
    );
    MSG_Add("CONFIGFILE_INTRO",
            "# This is the configuration file for DOSBox %s. (Please use the latest version of DOSBox)\n"
            "# Lines starting with a # are comment lines and are ignored by DOSBox.\n"
            "# They are used to (briefly) document the effect of each option.\n"
        "# To write out ALL options, use command 'config -all' with -wc or -writeconf options.\n");
    MSG_Add("CONFIG_SUGGESTED_VALUES", "Possible values");
}

int utf8_encode(char **ptr,char *fence,uint32_t code) {
    int uchar_size=1;
    char *p = *ptr;

    if (!p) return UTF8ERR_NO_ROOM;
    if (code >= (uint32_t)0x80000000UL) return UTF8ERR_INVALID;
    if (p >= fence) return UTF8ERR_NO_ROOM;

    if (code >= 0x4000000) uchar_size = 6;
    else if (code >= 0x200000) uchar_size = 5;
    else if (code >= 0x10000) uchar_size = 4;
    else if (code >= 0x800) uchar_size = 3;
    else if (code >= 0x80) uchar_size = 2;

    if ((p+uchar_size) > fence) return UTF8ERR_NO_ROOM;

    switch (uchar_size) {
        case 1: *p++ = (char)code;
            break;
        case 2: *p++ = (char)(0xC0 | (code >> 6));
            *p++ = (char)(0x80 | (code & 0x3F));
            break;
        case 3: *p++ = (char)(0xE0 | (code >> 12));
            *p++ = (char)(0x80 | ((code >> 6) & 0x3F));
            *p++ = (char)(0x80 | (code & 0x3F));
            break;
        case 4: *p++ = (char)(0xF0 | (code >> 18));
            *p++ = (char)(0x80 | ((code >> 12) & 0x3F));
            *p++ = (char)(0x80 | ((code >> 6) & 0x3F));
            *p++ = (char)(0x80 | (code & 0x3F));
            break;
        case 5: *p++ = (char)(0xF8 | (code >> 24));
            *p++ = (char)(0x80 | ((code >> 18) & 0x3F));
            *p++ = (char)(0x80 | ((code >> 12) & 0x3F));
            *p++ = (char)(0x80 | ((code >> 6) & 0x3F));
            *p++ = (char)(0x80 | (code & 0x3F));
            break;
        case 6: *p++ = (char)(0xFC | (code >> 30));
            *p++ = (char)(0x80 | ((code >> 24) & 0x3F));
            *p++ = (char)(0x80 | ((code >> 18) & 0x3F));
            *p++ = (char)(0x80 | ((code >> 12) & 0x3F));
            *p++ = (char)(0x80 | ((code >> 6) & 0x3F));
            *p++ = (char)(0x80 | (code & 0x3F));
            break;
    };

    *ptr = p;
    return 0;
}

int utf8_decode(const char **ptr,const char *fence) {
    const char *p = *ptr;
    int uchar_size=1;
    int ret = 0,c;

    if (!p) return UTF8ERR_NO_ROOM;
    if (p >= fence) return UTF8ERR_NO_ROOM;

    ret = (unsigned char)(*p);
    if (ret >= 0xFE) { p++; return UTF8ERR_INVALID; }
    else if (ret >= 0xFC) uchar_size=6;
    else if (ret >= 0xF8) uchar_size=5;
    else if (ret >= 0xF0) uchar_size=4;
    else if (ret >= 0xE0) uchar_size=3;
    else if (ret >= 0xC0) uchar_size=2;
    else if (ret >= 0x80) { p++; return UTF8ERR_INVALID; }

    if ((p+uchar_size) > fence)
        return UTF8ERR_NO_ROOM;

    switch (uchar_size) {
        case 1: p++;
            break;
        case 2: ret = (ret&0x1F)<<6; p++;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= c&0x3F;
            break;
        case 3: ret = (ret&0xF)<<12; p++;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<6;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= c&0x3F;
            break;
        case 4: ret = (ret&0x7)<<18; p++;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<12;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<6;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= c&0x3F;
            break;
        case 5: ret = (ret&0x3)<<24; p++;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<18;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<12;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<6;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= c&0x3F;
            break;
        case 6: ret = (ret&0x1)<<30; p++;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<24;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<18;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<12;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<6;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= c&0x3F;
            break;
    };

    *ptr = p;
    return ret;
}

int utf16le_encode(char **ptr,char *fence,uint32_t code) {
    char *p = *ptr;

    if (!p) return UTF8ERR_NO_ROOM;
    if (code > 0x10FFFF) return UTF8ERR_INVALID;
    if (code > 0xFFFF) { /* UTF-16 surrogate pair */
        uint32_t lo = (code - 0x10000) & 0x3FF;
        uint32_t hi = ((code - 0x10000) >> 10) & 0x3FF;
        if ((p+2+2) > fence) return UTF8ERR_NO_ROOM;
        *p++ = (char)( (hi+0xD800)       & 0xFF);
        *p++ = (char)(((hi+0xD800) >> 8) & 0xFF);
        *p++ = (char)( (lo+0xDC00)       & 0xFF);
        *p++ = (char)(((lo+0xDC00) >> 8) & 0xFF);
    }
    else if ((code&0xF800) == 0xD800) { /* do not allow accidental surrogate pairs (0xD800-0xDFFF) */
        return UTF8ERR_INVALID;
    }
    else {
        if ((p+2) > fence) return UTF8ERR_NO_ROOM;
        *p++ = (char)( code       & 0xFF);
        *p++ = (char)((code >> 8) & 0xFF);
    }

    *ptr = p;
    return 0;
}

int utf16le_decode(const char **ptr,const char *fence) {
    const char *p = *ptr;
    unsigned int ret,b=2;

    if (!p) return UTF8ERR_NO_ROOM;
    if ((p+1) >= fence) return UTF8ERR_NO_ROOM;

    ret = (unsigned char)p[0];
    ret |= ((unsigned int)((unsigned char)p[1])) << 8;
    if (ret >= 0xD800U && ret <= 0xDBFFU)
        b=4;
    else if (ret >= 0xDC00U && ret <= 0xDFFFU)
        { p++; return UTF8ERR_INVALID; }

    if ((p+b) > fence)
        return UTF8ERR_NO_ROOM;

    p += 2;
    if (ret >= 0xD800U && ret <= 0xDBFFU) {
        /* decode surrogate pair */
        unsigned int hi = ret & 0x3FFU;
        unsigned int lo = (unsigned char)p[0];
        lo |= ((unsigned int)((unsigned char)p[1])) << 8;
        p += 2;
        if (lo < 0xDC00U || lo > 0xDFFFU) return UTF8ERR_INVALID;
        lo &= 0x3FFU;
        ret = ((hi << 10U) | lo) + 0x10000U;
    }

    *ptr = p;
    return (int)ret;
}

