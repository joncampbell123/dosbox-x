
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

void pic_setirq(REG8 irq) {
    PIC_ActivateIRQ(irq);
}

void pic_resetirq(REG8 irq) {
    PIC_DeActivateIRQ(irq);
}

int pc98_fm_irq = 3; /* TODO: Make configurable */
unsigned int pc98_fm26_base = 0x088; /* TODO: Make configurable */
unsigned int pc98_fm86_base = 0x188; /* TODO: Make configurable */

#include "sound.h"
#include "fmboard.h"

// wrapper for fmtimer events
void fmport_a(NEVENTITEM item);
void fmport_b(NEVENTITEM item);

static void fmport_a_pic_event(Bitu val) {
    fmport_a(NULL);
}

static void fmport_b_pic_event(Bitu val) {
    fmport_b(NULL);
}

extern "C" void pc98_fm_dosbox_fmtimer_setevent(unsigned int n,double dt) {
    PIC_EventHandler func = n ? fmport_b_pic_event : fmport_a_pic_event;

    PIC_RemoveEvents(func);
    PIC_AddEvent(func,dt);
}

extern "C" void pc98_fm_dosbox_fmtimer_clearevent(unsigned int n) {
    PIC_RemoveEvents(n ? fmport_b_pic_event : fmport_a_pic_event);
}

// mixer callback

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
#if defined(SUPPORT_PX)
    pcmmix_getpcm((PCMMIX)(&rhythm2), (SINT32*)MixTemp, s);
    pcmmix_getpcm((PCMMIX)(&rhythm3), (SINT32*)MixTemp, s);
#endif	// defined(SUPPORT_PX)

    adpcm_getpcm(&adpcm, (SINT32*)MixTemp, s);
#if defined(SUPPORT_PX)
    adpcm_getpcm(&adpcm2, (SINT32*)MixTemp, s);
    adpcm_getpcm(&adpcm3, (SINT32*)MixTemp, s);
#endif	// defined(SUPPORT_PX)

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

