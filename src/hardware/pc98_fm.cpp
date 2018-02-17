
#include "dosbox.h"
#include "setup.h"
#include "video.h"
#include "pic.h"
#include "vga.h"
#include "inout.h"
#include "programs.h"
#include "support.h"
#include "setup.h"
#include "timer.h"
#include "mem.h"
#include "util_units.h"
#include "control.h"
#include "pc98_cg.h"
#include "pc98_dac.h"
#include "pc98_gdc.h"
#include "pc98_gdc_const.h"
#include "mixer.h"

#include <string.h>
#include <stdlib.h>
#include <string>
#include <stdio.h>

using namespace std;

#include <map>

#include "np2glue.h"

MixerChannel *pc98_mixer = NULL;

NP2CFG pccore;

extern "C" unsigned char *CGetMemBase() {
    return MemBase;
}

extern "C" void sound_sync(void) {
    if (pc98_mixer) pc98_mixer->FillUp();
}

extern "C" void _TRACEOUT(const char *fmt,...) {
    va_list va;
    char tmp[512];

    va_start(va,fmt);

    vsnprintf(tmp,sizeof(tmp),fmt,va);
    LOG_MSG("PC98FM TRACEOUT: %s",tmp);

    va_end(va);
}

void getbiospath(OEMCHAR *path, const OEMCHAR *fname, int maxlen) {
    LOG_MSG("PC98FM getbiospath fname='%s'",fname);
    snprintf(path,maxlen,fname);
}

REG8 joymng_getstat(void) {
    return 0xFF;//TODO
}

REG8 keystat_getjoy(void) {
    return 0xFF;//TODO
}

struct CBUS4PORT {
    IOINP       inp;
    IOOUT       out;

    CBUS4PORT() : inp(NULL), out(NULL) { }
};

static std::map<UINT,CBUS4PORT> cbuscore_map;

void pc98_fm86_write(Bitu port,Bitu val,Bitu iolen) {
    auto &cbusm = cbuscore_map[port];
    auto &func = cbusm.out;
    if (func) func(port,val);
}

Bitu pc98_fm86_read(Bitu port,Bitu iolen) {
    auto &cbusm = cbuscore_map[port];
    auto &func = cbusm.inp;
    if (func) return func(port);
    return ~0;
}

// four I/O ports, 2 ports apart
void cbuscore_attachsndex(UINT port, const IOOUT *out, const IOINP *inp) {
    LOG_MSG("cbuscore_attachsndex(port=0x%x)",port);

    for (unsigned int i=0;i < 4;i++) {
        auto &cbusm = cbuscore_map[port+(i*2)];

        IO_RegisterReadHandler(port+(i*2),pc98_fm86_read,IO_MB);
        cbusm.inp = inp[i];

        IO_RegisterWriteHandler(port+(i*2),pc98_fm86_write,IO_MB);
        cbusm.out = out[i];
    }
}

int pc98_fm_irq = 3; /* TODO: Make configurable */
unsigned int pc98_fm26_base = 0x088; /* TODO: Make configurable */
unsigned int pc98_fm86_base = 0x188; /* TODO: Make configurable */

#include "sound.h"
#include "fmboard.h"
//#include "opngen.h"       already from fmboard.h
//#include "pcm86.h"        already from fmboard.h
//#include "psggen.h"       already from fmboard.h
//#include "adpcm.h"        already from fmboard.h
//#include "tms3631.h"      already from fmboard.h
//#include "fmtimer.h"      already from fmboard.h

//-----------------------------------------------------------
// Neko Project II: sound/fmtimer.h
//-----------------------------------------------------------

//void fmport_a(NEVENTITEM item);
//void fmport_b(NEVENTITEM item);

/////// from sound/fmboard.c

//-----------------------------------------------------------
// Neko Project II: sound/fmtimer.c
//-----------------------------------------------------------

static const UINT8 irqtable[4] = {0x03, 0x0d, 0x0a, 0x0c};

static void set_fmtimeraevent(BOOL absolute);
static void set_fmtimerbevent(BOOL absolute);
void fmport_a(NEVENTITEM item);
void fmport_b(NEVENTITEM item);

bool FMPORT_EventA_set = false;
bool FMPORT_EventB_set = false;

static void FMPORT_EventA(Bitu val) {
    FMPORT_EventA_set = false;
    fmport_a(NULL);
    (void)val;
}

static void FMPORT_EventB(Bitu val) {
    FMPORT_EventB_set = false;
    fmport_b(NULL);
    (void)val;
}


void fmport_a(NEVENTITEM item) {

    BOOL	intreq = FALSE;

    intreq = pcm86gen_intrq();
    if (fmtimer.reg & 0x04) {
        fmtimer.status |= 0x01;
        intreq = TRUE;
    }

    if (intreq)
        PIC_ActivateIRQ(pc98_fm_irq);

    set_fmtimeraevent(FALSE);
}

void fmport_b(NEVENTITEM item) {

    BOOL	intreq = FALSE;

    intreq = pcm86gen_intrq();
    if (fmtimer.reg & 0x08) {
        fmtimer.status |= 0x02;
        intreq = TRUE;
    }

    if (intreq)
        PIC_ActivateIRQ(pc98_fm_irq);

    set_fmtimerbevent(FALSE);
}

static void set_fmtimeraevent(BOOL absolute) {

	SINT32	l;
    double dt;

	l = 18 * (1024 - fmtimer.timera);
    dt = ((double)l * 1000) / 1000000; // FIXME: GUESS!!!!
//	if (PIT_TICK_RATE == PIT_TICK_RATE_PC98_8MHZ) {	 // 4MHz
//		l = (l * 1248 / 625) * pccore.multiple;    <- NOTE: This becomes l * 1996800Hz * multiple
//	}
//	else {										// 5MHz
//		l = (l * 1536 / 625) * pccore.multiple;    <- NOTE: This becomes l * 2457600Hz * multiple
//	}
//	TRACEOUT(("FMTIMER-A: %08x-%d", l, absolute));
//	nevent_set(NEVENT_FMTIMERA, l, fmport_a, absolute);

    PIC_RemoveEvents(FMPORT_EventA);
    PIC_AddEvent(FMPORT_EventA, dt);
    FMPORT_EventA_set = true;

    (void)l;
}

static void set_fmtimerbevent(BOOL absolute) {

	SINT32	l;
    double dt;

	l = 288 * (256 - fmtimer.timerb);
    dt = ((double)l * 1000) / 1000000; // FIXME: GUESS!!!!
//	if (PIT_TICK_RATE == PIT_TICK_RATE_PC98_8MHZ) {	 // 4MHz
//		l = (l * 1248 / 625) * pccore.multiple;
//	}
//	else {										// 5MHz
//		l = (l * 1536 / 625) * pccore.multiple;
//	}
//	TRACEOUT(("FMTIMER-B: %08x-%d", l, absolute));
//	nevent_set(NEVENT_FMTIMERB, l, fmport_b, absolute);

    PIC_RemoveEvents(FMPORT_EventB);
    PIC_AddEvent(FMPORT_EventB, dt);
    FMPORT_EventB_set = true;

    (void)l;
}

void fmtimer_reset(UINT irq) {

	ZeroMemory(&fmtimer, sizeof(fmtimer));
	fmtimer.intr = irq & 0xc0;
	fmtimer.intdisabel = irq & 0x10;
	fmtimer.irq = irqtable[irq >> 6];
//	pic_registext(fmtimer.irq);
}

void fmtimer_setreg(UINT reg, REG8 value) {

//	TRACEOUT(("fm %x %x [%.4x:%.4x]", reg, value, CPU_CS, CPU_IP));

	switch(reg) {
		case 0x24:
			fmtimer.timera = (value << 2) + (fmtimer.timera & 3);
			break;

		case 0x25:
			fmtimer.timera = (fmtimer.timera & 0x3fc) + (value & 3);
			break;

		case 0x26:
			fmtimer.timerb = value;
			break;

		case 0x27:
			fmtimer.reg = value;
			fmtimer.status &= ~((value & 0x30) >> 4);
			if (value & 0x01) {
                if (!FMPORT_EventA_set)
                    set_fmtimeraevent(0);
            }
            else {
                FMPORT_EventA_set = false;
                PIC_RemoveEvents(FMPORT_EventA);
            }

            if (value & 0x02) {
                if (!FMPORT_EventB_set)
                    set_fmtimerbevent(0);
            }
            else {
                FMPORT_EventB_set = false;
                PIC_RemoveEvents(FMPORT_EventB);
            }

			if (!(value & 0x03)) {
                PIC_DeActivateIRQ(pc98_fm_irq);
            }
			break;
	}
}


/////////////////////////////////////////////////////////////

static void pc98_mix_CallBack(Bitu len) {
    unsigned int s = len;

    if (s > (sizeof(MixTemp)/sizeof(Bit32s)/2))
        s = (sizeof(MixTemp)/sizeof(Bit32s)/2);

    memset(MixTemp,0,sizeof(MixTemp));

    opngen_getpcm(NULL, (SINT32*)MixTemp, s);
    tms3631_getpcm(&tms3631, (SINT32*)MixTemp, s);

    for (unsigned int i=0;i < 3;i++)
        psggen_getpcm(&__psg[i], (SINT32*)MixTemp, s);
 
    // NTS: _RHYTHM is a struct with the same initial layout as PCMMIX
    pcmmix_getpcm((PCMMIX)(&rhythm), (SINT32*)MixTemp, s);
    adpcm_getpcm(&adpcm, (SINT32*)MixTemp, s);
    pcm86gen_getpcm(NULL, (SINT32*)MixTemp, s);

    pc98_mixer->AddSamples_s32(s, (Bit32s*)MixTemp);
}

static bool pc98fm_init = false;

void PC98_FM_OnEnterPC98(Section *sec) {
    if (!pc98fm_init) {
        pc98fm_init = true;

        unsigned int rate = 44100;

        memset(&pccore,0,sizeof(pccore));
        pccore.samplingrate = 44100;
		pccore.baseclock = PIT_TICK_RATE;
		pccore.multiple = 1;
        for (unsigned int i=0;i < 6;i++) pccore.vol14[i] = 0xFF;
        pccore.vol_fm = 128;
        pccore.vol_ssg = 128;
        pccore.vol_adpcm = 128;
        pccore.vol_pcm = 128;
        pccore.vol_rhythm = 128;
        pccore.snd86opt = 0x01;

        //	fddmtrsnd_initialize(rate);
        //	beep_initialize(rate);
        //	beep_setvol(3);
        tms3631_initialize(rate);
        tms3631_setvol(pccore.vol14);
        opngen_initialize(rate);
        opngen_setvol(pccore.vol_fm);
        psggen_initialize(rate);
        psggen_setvol(pccore.vol_ssg);
        rhythm_initialize(rate);
        rhythm_setvol(pccore.vol_rhythm);
        adpcm_initialize(rate);
        adpcm_setvol(pccore.vol_adpcm);
        pcm86gen_initialize(rate);
        pcm86gen_setvol(pccore.vol_pcm);

        fmboard_reset(&np2cfg, 0x14);
        fmboard_extenable(true);

        fmboard_bind();

        // WARNING: Some parts of the borrowed code assume 44100, 22050, or 11025 and
        //          will misrender if given any other sample rate (especially the OPNA synth).

        pc98_mixer = MIXER_AddChannel(pc98_mix_CallBack, rate, "PC-98");
        pc98_mixer->Enable(true);
    }
}

