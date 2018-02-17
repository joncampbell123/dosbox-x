#include	"compiler.h"
#include	"cpucore.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"sound.h"
#include	"fmboard.h"


// サンプリングレートに8掛けた物
const UINT pcm86rate8[] = {352800, 264600, 176400, 132300,
							88200,  66150,  44010,  33075};

// 32,24,16,12, 8, 6, 4, 3 - 最少公倍数: 96
//  3, 4, 6, 8,12,16,24,32

static const UINT clk25_128[] = {
					0x00001bde, 0x00002527, 0x000037bb, 0x00004a4e,
					0x00006f75, 0x0000949c, 0x0000df5f, 0x00012938};
static const UINT clk20_128[] = {
					0x000016a4, 0x00001e30, 0x00002d48, 0x00003c60,
					0x00005a8f, 0x000078bf, 0x0000b57d, 0x0000f17d};


	PCM86CFG	pcm86cfg;


void pcm86gen_initialize(UINT rate) {

	pcm86cfg.rate = rate;
}

void pcm86gen_setvol(UINT vol) {

	pcm86cfg.vol = vol;
	pcm86gen_update();
}

void pcm86_reset(void) {

	ZeroMemory(&pcm86, sizeof(pcm86));
	pcm86.fifosize = 0x80;
	pcm86.dactrl = 0x32;
	pcm86.stepmask = (1 << 2) - 1;
	pcm86.stepbit = 2;
	pcm86.stepclock = (pccore.baseclock << 6);
	pcm86.stepclock /= 44100;
	pcm86.stepclock *= pccore.multiple;
	pcm86.rescue = (PCM86_RESCUE * 32) << 2;
}

void pcm86gen_update(void) {

	pcm86.volume = pcm86cfg.vol * pcm86.vol5;
	pcm86_setpcmrate(pcm86.fifo);
}

void pcm86_setpcmrate(REG8 val) {

	SINT32	rate;

	rate = pcm86rate8[val & 7];
	pcm86.stepclock = (pccore.baseclock << 6);
	pcm86.stepclock /= rate;
	pcm86.stepclock *= (pccore.multiple << 3);
	if (pcm86cfg.rate) {
		pcm86.div = (rate << (PCM86_DIVBIT - 3)) / pcm86cfg.rate;
		pcm86.div2 = (pcm86cfg.rate << (PCM86_DIVBIT + 3)) / rate;
	}
}

void pcm86_cb(NEVENTITEM item) {

	if (pcm86.reqirq) {
		sound_sync();
//		RECALC_NOWCLKP;
		if (pcm86.virbuf <= pcm86.fifosize) {
			pcm86.reqirq = 0;
			pcm86.irqflag = 1;
			pic_setirq(fmtimer.irq);
		}
		else {
			pcm86_setnextintr();
		}
	}
	(void)item;
}

void pcm86_setnextintr(void) {

	SINT32	cnt;
	SINT32	clk;

	if (pcm86.fifo & 0x80) {
		cnt = pcm86.virbuf - pcm86.fifosize;
		if (cnt > 0) {
			cnt += pcm86.stepmask;
			cnt >>= pcm86.stepbit;
//			cnt += 4;								// ちょっと延滞させる
			// ここで clk = pccore.realclock * cnt / 86pcm_rate
			// clk = ((pccore.baseclock / 86pcm_rate) * cnt) * pccore.multiple
			if (pccore.cpumode & CPUMODE_8MHZ) {
				clk = clk20_128[pcm86.fifo & 7];
			}
			else {
				clk = clk25_128[pcm86.fifo & 7];
			}
			// cntは最大 8000h で 32bitで収まるように…
			clk *= cnt;
			clk >>= 7;
//			clk++;						// roundup
			clk *= pccore.multiple;
			nevent_set(NEVENT_86PCM, clk, pcm86_cb, NEVENT_ABSOLUTE);
		}
	}
}

void SOUNDCALL pcm86gen_checkbuf(void) {

	long	bufs;
	UINT32	past;

	past = CPU_CLOCK + CPU_BASECLOCK - CPU_REMCLOCK;
	past <<= 6;
	past -= pcm86.lastclock;
	if (past >= pcm86.stepclock) {
		past = past / pcm86.stepclock;
		pcm86.lastclock += (past * pcm86.stepclock);
		RECALC_NOWCLKWAIT(past);
	}

	bufs = pcm86.realbuf - pcm86.virbuf;
	if (bufs < 0) {									// 処理落ちてる…
		bufs &= ~3;
		pcm86.virbuf += bufs;
		if (pcm86.virbuf <= pcm86.fifosize) {
			pcm86.reqirq = 0;
			pcm86.irqflag = 1;
			pic_setirq(fmtimer.irq);
		}
		else {
			pcm86_setnextintr();
		}
	}
	else {
		bufs -= PCM86_EXTBUF;
		if (bufs > 0) {
			bufs &= ~3;
			pcm86.realbuf -= bufs;
			pcm86.readpos += bufs;
		}
	}
}


BOOL pcm86gen_intrq(void) {

	if (pcm86.irqflag) {
		return(TRUE);
	}
	if (pcm86.fifo & 0x20) {
		sound_sync();
		if ((pcm86.reqirq) && (pcm86.virbuf <= pcm86.fifosize)) {
			pcm86.reqirq = 0;
			pcm86.irqflag = 1;
			return(TRUE);
		}
	}
	return(FALSE);
}

