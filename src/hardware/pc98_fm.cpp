
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

#include "np2glue.h"

MixerChannel *pc98_mixer = NULL;

NP2CFG pccore;

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

#if !defined(DISABLE_SOUND)

#ifdef __cplusplus
extern "C" {
#endif

static void	(*extfn)(REG8 enable);


extern	UINT32		usesound;
extern	OPN_T		opn;
extern	AMD98		amd98;
extern	MUSICGEN	musicgen;

	_FMTIMER	fmtimer;

extern	_TMS3631	tms3631;
extern	_FMTIMER	fmtimer;
extern	_OPNGEN		opngen;
extern	OPNCH		opnch[OPNCH_MAX];
extern	_PSGGEN		__psg[3];
extern	_RHYTHM		rhythm;
extern _PCM86       pcm86;
//extern	_CS4231		cs4231;

	UINT32		usesound;
	OPN_T		opn;
	AMD98		amd98;
	MUSICGEN	musicgen;

	_TMS3631	tms3631;
	_OPNGEN		opngen;
	OPNCH		opnch[OPNCH_MAX];
	_PSGGEN		__psg[3];
	_RHYTHM		rhythm;
    _ADPCM      adpcm;
    _PCM86      pcm86;
//	_CS4231		cs4231;

	OPN_T		opn2;
	OPN_T		opn3;
	_RHYTHM		rhythm2;
	_RHYTHM		rhythm3;

#define	psg1	__psg[0]
#define	psg2	__psg[1]
#define	psg3	__psg[2]

extern	OPN_T		opn2;
extern	OPN_T		opn3;
extern	_RHYTHM		rhythm2;
extern	_RHYTHM		rhythm3;


// ----

//static	REG8	rapids = 0;

// ----

	OPNCFG	opncfg;
#ifdef OPNGENX86
	char	envshift[EVC_ENT];
	char	sinshift[SIN_ENT];
#endif


extern	OPNCFG	opncfg;

// ----

void fmboard_extreg(void (*ext)(REG8 enable)) {

	extfn = ext;
}

void fmboard_extenable(REG8 enable) {

	if (extfn) {
		(*extfn)(enable);
	}
}



// ----

static void setfmregs(UINT8 *reg) {

	FillMemory(reg + 0x30, 0x60, 0xff);
	FillMemory(reg + 0x90, 0x20, 0x00);
	FillMemory(reg + 0xb0, 0x04, 0x00);
	FillMemory(reg + 0xb4, 0x04, 0xc0);
}

void fmtimer_reset(UINT irq);
void fmtimer_setreg(UINT reg, REG8 value);

static void extendchannel(REG8 enable) {

	opn.extend = enable;
	if (enable) {
		opn.channels = 6;
		opngen_setcfg(6, OPN_STEREO | 0x007);
	}
	else {
		opn.channels = 3;
		opngen_setcfg(3, OPN_MONORAL | 0x007);
		rhythm_setreg(&rhythm, 0x10, 0xff);
	}
}

void board86_reset(const NP2CFG *pConfig) {

	fmtimer_reset(0/*IRQ3*/);
	opngen_setcfg(3, OPN_STEREO | 0x038);
//	if (pConfig->snd86opt & 2) {
//		soundrom_load(0xcc000, OEMTEXT("86"));
//	}
	opn.base = 0x100;//(pConfig->snd86opt & 0x01)?0x000:0x100;
	fmboard_extreg(extendchannel);
}


void fmboard_reset(const NP2CFG *pConfig, UINT32 type) {

//	UINT8	cross;

//	soundrom_reset();
//	beep_reset();												// ver0.27a
//	cross = np2cfg.snd_x;										// ver0.30

    LOG_MSG("FM reset");

	extfn = NULL;
	ZeroMemory(&opn, sizeof(opn));
	setfmregs(opn.reg + 0x000);
	setfmregs(opn.reg + 0x100);
	setfmregs(opn.reg + 0x200);
	setfmregs(opn.reg + 0x300);
	opn.reg[0xff] = 0x01;
	opn.channels = 3;
	opn.adpcmmask = (UINT8)~(0x1c);

	ZeroMemory(&opn2, sizeof(opn2));
	setfmregs(opn2.reg + 0x000);
	setfmregs(opn2.reg + 0x100);
	setfmregs(opn2.reg + 0x200);
	setfmregs(opn2.reg + 0x300);
	opn2.reg[0xff] = 0x01;
	opn2.channels = 3;
	opn2.adpcmmask = (UINT8)~(0x1c);

	ZeroMemory(&opn3, sizeof(opn3));
	setfmregs(opn3.reg + 0x000);
	setfmregs(opn3.reg + 0x100);
	setfmregs(opn3.reg + 0x200);
	setfmregs(opn3.reg + 0x300);
	opn3.reg[0xff] = 0x01;
	opn3.channels = 3;
	opn3.adpcmmask = (UINT8)~(0x1c);

	ZeroMemory(&musicgen, sizeof(musicgen));
	ZeroMemory(&amd98, sizeof(amd98));

	tms3631_reset(&tms3631);
	opngen_reset();
	psggen_reset(&psg1);
	psggen_reset(&psg2);
	psggen_reset(&psg3);
	rhythm_reset(&rhythm);
	rhythm_reset(&rhythm2);
	rhythm_reset(&rhythm3);
	adpcm_reset(&adpcm);
//	adpcm_reset(&adpcm2);
//	adpcm_reset(&adpcm3);
	pcm86_reset();
//    cs4231_reset();

    board86_reset(pConfig);

    usesound = type;
//    soundmng_setreverse(0);
	opngen_setVR(0,0);//pConfig->spb_vrc, pConfig->spb_vrl);??
}

void board86c_bind(void);

void fmboard_bind(void) {
    board86c_bind();

//    sound_streamregist(&beep, (SOUNDCB)beep_getpcm);
}


// ----

void fmboard_fmrestore(REG8 chbase, UINT bank) {

	REG8	i;
const UINT8	*reg;

	reg = opn.reg + (bank * 0x100);
	for (i=0x30; i<0xa0; i++) {
		opngen_setreg(chbase, i, reg[i]);
	}
	for (i=0xb7; i>=0xa0; i--) {
		opngen_setreg(chbase, i, reg[i]);
	}
	for (i=0; i<3; i++) {
		opngen_keyon(chbase + i, opngen.keyreg[chbase + i]);
	}
}

void fmboard_rhyrestore(RHYTHM rhy, UINT bank) {

const UINT8	*reg;

	reg = opn.reg + (bank * 0x100);
	rhythm_setreg(rhy, 0x11, reg[0x11]);
	rhythm_setreg(rhy, 0x18, reg[0x18]);
	rhythm_setreg(rhy, 0x19, reg[0x19]);
	rhythm_setreg(rhy, 0x1a, reg[0x1a]);
	rhythm_setreg(rhy, 0x1b, reg[0x1b]);
	rhythm_setreg(rhy, 0x1c, reg[0x1c]);
	rhythm_setreg(rhy, 0x1d, reg[0x1d]);
}


void fmboard_fmrestore2(OPN_T* pOpn, REG8 chbase, UINT bank) {

	REG8	i;
const UINT8	*reg;

	reg = pOpn->reg + (bank * 0x100);
	for (i=0x30; i<0xa0; i++) {
		opngen_setreg(chbase, i, reg[i]);
	}
	for (i=0xb7; i>=0xa0; i--) {
		opngen_setreg(chbase, i, reg[i]);
	}
	for (i=0; i<3; i++) {
		opngen_keyon(chbase + i, opngen.keyreg[chbase + i]);
	}
}

void fmboard_rhyrestore2(OPN_T* pOpn, RHYTHM rhy, UINT bank) {

const UINT8	*reg;

	reg = pOpn->reg + (bank * 0x100);
	rhythm_setreg(rhy, 0x11, reg[0x11]);
	rhythm_setreg(rhy, 0x18, reg[0x18]);
	rhythm_setreg(rhy, 0x19, reg[0x19]);
	rhythm_setreg(rhy, 0x1a, reg[0x1a]);
	rhythm_setreg(rhy, 0x1b, reg[0x1b]);
	rhythm_setreg(rhy, 0x1c, reg[0x1c]);
	rhythm_setreg(rhy, 0x1d, reg[0x1d]);
}

void fmboard_extreg(void (*ext)(REG8 enable));
void fmboard_extenable(REG8 enable);

void fmboard_reset(const NP2CFG *pConfig, UINT32 type);
void fmboard_bind(void);

void fmboard_fmrestore(REG8 chbase, UINT bank);
void fmboard_rhyrestore(RHYTHM rhy, UINT bank);

void fmboard_fmrestore2(OPN_T* pOpn, REG8 chbase, UINT bank);
void fmboard_rhyrestore2(OPN_T* pOpn, RHYTHM rhy, UINT bank);

#ifdef __cplusplus
}
#endif

#else

#define	fmboard_reset(t)
#define	fmboard_bind()

#endif

enum {
	NORMAL2608	= 0,
	EXTEND2608	= 1
};

void S98_init(void);
void S98_trash(void);
int S98_open(const OEMCHAR *filename);
void S98_close(void);
void S98_put(REG8 module, UINT addr, REG8 data);
void S98_sync(void);

void S98_put(REG8 module, UINT addr, REG8 data) {
}

static const OEMCHAR file_2608bd[] = OEMTEXT("2608_bd.wav");
static const OEMCHAR file_2608sd[] = OEMTEXT("2608_sd.wav");
static const OEMCHAR file_2608top[] = OEMTEXT("2608_top.wav");
static const OEMCHAR file_2608hh[] = OEMTEXT("2608_hh.wav");
static const OEMCHAR file_2608tom[] = OEMTEXT("2608_tom.wav");
static const OEMCHAR file_2608rim[] = OEMTEXT("2608_rim.wav");

typedef struct {
	UINT	rate;
	UINT	pcmexist;
//	PMIXDAT	pcm[6];
	UINT	vol;
	UINT	voltbl[96];
} RHYTHMCFG;

static	RHYTHMCFG	rhythmcfg;

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

void board86c_bind(void) {

	fmboard_fmrestore(0, 0);
	fmboard_fmrestore(3, 1);
	psggen_restore(&psg1);
	fmboard_rhyrestore(&rhythm, 0);
//	sound_streamregist(&opngen, (SOUNDCB)opngen_getpcm);
//	sound_streamregist(&psg1, (SOUNDCB)psggen_getpcm);
	rhythm_bind(&rhythm);
//	sound_streamregist(&adpcm, (SOUNDCB)adpcm_getpcm);
	pcm86io_bind();
//	cbuscore_attachsndex(0x188 + opn.base, opnac_o, opnac_i);
}

void pc98_fm86_write(Bitu port,Bitu val,Bitu iolen) {
    unsigned char dat = val;

//    LOG_MSG("PC-98 FM: Write port 0x%x val 0x%x",(unsigned int)port,(unsigned int)val);

    switch (port+0x88-pc98_fm86_base) {
        case 0x88:
            opn.addr = dat;
            opn.data = dat;
            break;
        case 0x8A:
            {
                UINT    addr;

                opn.data = dat;
                addr = opn.addr;
                if (addr >= 0x100) {
                    return;
                }
                S98_put(NORMAL2608, addr, dat);
                if (addr < 0x10) {
                    if (addr != 0x0e) {
                        psggen_setreg(&psg1, addr, dat);
                    }
                }
                else {
                    if (addr < 0x20) {
                        if (opn.extend) {
                            rhythm_setreg(&rhythm, addr, dat);
                        }
                    }
                    else if (addr < 0x30) {
                        if (addr == 0x28) {
                            if ((dat & 0x0f) < 3) {
                                opngen_keyon(dat & 0x0f, dat);
                            }
                            else if (((dat & 0x0f) != 3) &&
                                    ((dat & 0x0f) < 7)) {
                                opngen_keyon((dat & 0x07) - 1, dat);
                            }
                        }
                        else {
                            fmtimer_setreg(addr, dat);
                            if (addr == 0x27) {
                                opnch[2].extop = dat & 0xc0;
                            }
                        }
                    }
                    else if (addr < 0xc0) {
                        opngen_setreg(0, addr, dat);
                    }
                    opn.reg[addr] = dat;
                }
            }
            break;
        case 0x8C:
            if (opn.extend) {
                opn.addr = dat + 0x100;
                opn.data = dat;
            }
            break;
        case 0x8E:
            {
                UINT    addr;

                if (!opn.extend) {
                    return;
                }
                opn.data = dat;
                addr = opn.addr - 0x100;
                if (addr >= 0x100) {
                    return;
                }
                S98_put(EXTEND2608, addr, dat);
                opn.reg[addr + 0x100] = dat;
                if (addr >= 0x30) {
                    opngen_setreg(3, addr, dat);
                }
                else {
                    if (addr == 0x10) {
                        if (!(dat & 0x80)) {
                            opn.adpcmmask = ~(dat & 0x1c);
                        }
                    }
                }
                (void)port;
            }
            break;
        default:
            LOG_MSG("PC-98 FM: Write port 0x%x val 0x%x unknown",(unsigned int)port,(unsigned int)val);
            break;
    };
}

Bitu pc98_fm86_read(Bitu port,Bitu iolen) {
//    LOG_MSG("PC-98 FM: Read port 0x%x",(unsigned int)port);

    switch (port+0x88-pc98_fm86_base) {
        case 0x88:
            return fmtimer.status;
        case 0x8A:
            {
                UINT addr;

                addr = opn.addr;
                if (addr == 0x0e) {
                    return 0x3F; // NTS: Returning 0x00 causes games to think a joystick is attached
//                    return(fmboard_getjoy(&psg1));
                }
                else if (addr < 0x10) {
                    return(psggen_getreg(&psg1, addr));
                }
                else if (addr == 0xff) {
                    return(1);
                }
            }
            return(opn.data);
        case 0x8C:
            if (opn.extend) {
                return((fmtimer.status & 3) | (opn.adpcmmask & 8));
            }
            (void)port;
            return(0xff);
        case 0x8E:
            if (opn.extend) {
                UINT addr = opn.addr - 0x100;
                if ((addr == 0x08) || (addr == 0x0f)) {
                    return(opn.reg[addr + 0x100]);
                }
                return(opn.data);
            }
            (void)port;
            return(0xff);
        default:
            LOG_MSG("PC-98 FM: Read port 0x%x unknown",(unsigned int)port);
            break;
    };

    return ~0;
}

static void pc98_mix_CallBack(Bitu len) {
    unsigned int s = len;

    if (s > (sizeof(MixTemp)/sizeof(Bit32s)/2))
        s = (sizeof(MixTemp)/sizeof(Bit32s)/2);

    memset(MixTemp,0,sizeof(MixTemp));

    opngen_getpcm(NULL, (SINT32*)MixTemp, s);
    tms3631_getpcm(&tms3631, (SINT32*)MixTemp, s);

    for (unsigned int i=0;i < 3;i++)
        psggen_getpcm(&__psg[i], (SINT32*)MixTemp, s);
    
//    rhythm_getpcm(NULL, (SINT32*)MixTemp, s); FIXME
//    adpcm_getpcm(NULL, (SINT32*)MixTemp, s); FIXME
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

        // TODO:
        //  - Give the user an option in dosbox.conf to enable/disable FM emulation
        //  - Give the user a choice which board to emulate (the borrowed code can emulate 10 different cards)
        //  - Give the user a choice which IRQ to attach it to
        //  - Give the user a choice of base I/O address (0x088 or 0x188)
        //  - Move this code out into it's own file. This is SOUND code. It does not belong in vga.cpp.
        //  - Register the TMS3631, OPNA, PSG, RHYTHM, etc. outputs as individual mixer channels, where
        //    each can then run at their own sample rate, and the user can use DOSBox-X mixer controls to
        //    set volume, record individual tracks with WAV capture, etc.
        //  - Cleanup this code, organize it.
        //  - Make sure this code clearly indicates that it was borrowed and adapted from Neko Project II and
        //    ported to DOSBox-X. I cannot take credit for this code, I can only take credit for porting it
        //    and future refinements in this project.
        LOG_MSG("Initializing FM board at base 0x%x",pc98_fm86_base);
        for (unsigned int i=0;i < 8;i += 2) {
            IO_RegisterWriteHandler(pc98_fm86_base+i,pc98_fm86_write,IO_MB);
            IO_RegisterReadHandler(pc98_fm86_base+i,pc98_fm86_read,IO_MB);
        }

        // WARNING: Some parts of the borrowed code assume 44100, 22050, or 11025 and
        //          will misrender if given any other sample rate (especially the OPNA synth).

        pc98_mixer = MIXER_AddChannel(pc98_mix_CallBack, rate, "PC-98");
        pc98_mixer->Enable(true);

        fmboard_reset(NULL, 0x14);
        fmboard_extenable(true);

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

        board86c_bind();
    }
}

