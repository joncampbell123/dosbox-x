
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

#include <string.h>
#include <stdlib.h>
#include <string>
#include <stdio.h>

using namespace std;

#include "np2glue.h"

MixerChannel *pc98_mixer = NULL;

NP2CFG      np2_cfg = {0};

static bool pc98fm_init = false;

// NP2 GLUE
// from the source: attach 4 I/O handlers, input and output, to I/O ports (NP2)
void cbuscore_attachsndex(UINT port, const IOOUT *out, const IOINP *inp) {

	UINT	i;
	IOOUT	outfn;
	IOINP	inpfn;

	for (i=0; i<4; i++) {
		outfn = out[i];
		if (outfn) {
			iocore_attachsndout(port, outfn);
		}
		inpfn = inp[i];
		if (inpfn) {
			iocore_attachsndinp(port, inpfn);
		}
		port += 2;
	}
}

// NP2 GLUE
// attach a sound I/O port, with a 12-bit decode
BRESULT iocore_attachsndout(UINT port, IOOUT func) {

	UINT num = port;

    do {
        IO_RegisterWriteHandler(num,func,IO_MB,1);
//		attachout(iocore.base[num], port & 0xff, func);
        num += 0x1000;
    } while(num < 0x10000);

	return SUCCESS;
}

BRESULT iocore_attachsndinp(UINT port, IOINP func) {

	UINT num = port;

    do {
        IO_RegisterReadHandler(num,func,IO_MB,1);
//      attachinp(iocore.base[num], port & 0xff, func);
        num += 0x1000;
    } while(num < 0x10000);

	return SUCCESS;
}

void PC98_FM_OnEnterPC98(Section *sec) {
    if (!pc98fm_init) {
        pc98fm_init = true;

        LOG_MSG("Initializing PC-98 FM emulation");

        np2_cfg.baseclock = PIT_TICK_RATE;
	    np2_cfg.multiple = 1;               // doesn't matter
        np2_cfg.samplingrate = 44100;       // a lot of the code can only do 11025, 22050, or 44100
	    np2_cfg.snd86opt = 0;

        for (unsigned int i=0;i < 6;i++) np2_cfg.vol14[i] = 0xFF; // full volume (right??)

        np2_cfg.vol_fm = 128;
        np2_cfg.vol_ssg = 128;
        np2_cfg.vol_adpcm = 128;
        np2_cfg.vol_pcm = 128;
        np2_cfg.vol_rhythm = 128;
    }
}

