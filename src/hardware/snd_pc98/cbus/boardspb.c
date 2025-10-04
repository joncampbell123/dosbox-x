#include	"compiler.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"cbuscore.h"
#include	"boardspb.h"
#include	"sound.h"
#include	"fmboard.h"
#include	"s98.h"


static void IOOUTCALL spb_o188(UINT port, REG8 dat) {

	opn.addr = dat;
	opn.data = dat;
	(void)port;
}

static void IOOUTCALL spb_o18a(UINT port, REG8 dat) {

	UINT	addr;

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
			rhythm_setreg(&rhythm, addr, dat);
		}
		else if (addr < 0x30) {
			if (addr == 0x28) {
				if ((dat & 0x0f) < 3) {
					opngen_keyon(dat & 0x0f, dat);
				}
				else if (((dat & 0x0f) != 3) &&
						((dat & 0x0f) < 7)) {
					opngen_keyon((dat & 0x0f) - 1, dat);
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
	(void)port;
}

static void IOOUTCALL spb_o18c(UINT port, REG8 dat) {

	opn.addr = dat + 0x100;
	opn.data = dat;
	(void)port;
}

static void IOOUTCALL spb_o18e(UINT port, REG8 dat) {

	UINT	addr;

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
	else if (addr < 0x12) {
		adpcm_setreg(&adpcm, addr, dat);
	}
	(void)port;
}

static REG8 IOINPCALL spb_i188(UINT port) {

	(void)port;
	return((fmtimer.status & 3) | adpcm_status(&adpcm));
}

static REG8 IOINPCALL spb_i18a(UINT port) {

	UINT	addr;

	addr = opn.addr;
	if (addr == 0x0e) {
		return(fmboard_getjoy(&psg1));
	}
	else if (addr < 0x10) {
		return(psggen_getreg(&psg1, addr));
	}
	else if (addr == 0xff) {
		return(1);
	}
	else {
		(void)port;
		return(opn.data);
	}
}

static REG8 IOINPCALL spb_i18e(UINT port) {

	UINT	addr;

	addr = opn.addr - 0x100;
	if (addr == 0x08) {
		return(adpcm_readsample(&adpcm));
	}
	else if (addr == 0x0f) {
		return(opn.reg[addr + 0x100]);
	}
	else {
		(void)port;
		return(opn.data);
	}
}


// ---- spark board

static void IOOUTCALL spr_o588(UINT port, REG8 dat) {

	opn.addr2 = dat;
//	opn.data2 = dat;
	(void)port;
}

static void IOOUTCALL spr_o58a(UINT port, REG8 dat) {

	UINT	addr;

//	opn.data2 = dat;
	addr = opn.addr2;
	if (addr >= 0x100) {
		return;
	}
	if (addr < 0x30) {
		if (addr == 0x28) {
			if ((dat & 0x0f) < 3) {
				opngen_keyon((dat & 0x0f) + 6, dat);
			}
			else if (((dat & 0x0f) != 3) &&
					((dat & 0x0f) < 7)) {
				opngen_keyon((dat & 0x0f) + 5, dat);
			}
		}
		else {
			if (addr == 0x27) {
				opnch[8].extop = dat & 0xc0;
			}
		}
	}
	else if (addr < 0xc0) {
		opngen_setreg(6, addr, dat);
	}
	opn.reg[addr + 0x200] = dat;
	(void)port;
}

static void IOOUTCALL spr_o58c(UINT port, REG8 dat) {

	opn.addr2 = dat + 0x100;
//	opn.data2 = dat;
	(void)port;
}

static void IOOUTCALL spr_o58e(UINT port, REG8 dat) {

	UINT	addr;

//	opn.data2 = dat;
	addr = opn.addr2 - 0x100;
	if (addr >= 0x100) {
		return;
	}
	opn.reg[addr + 0x300] = dat;
	if (addr >= 0x30) {
		opngen_setreg(9, addr, dat);
	}
	(void)port;
}

static REG8 IOINPCALL spr_i588(UINT port) {

	(void)port;
	return(fmtimer.status);
}

static REG8 IOINPCALL spr_i58a(UINT port) {

	UINT	addr;

	addr = opn.addr2;
	if ((addr >= 0x20) && (addr < 0xff)) {
		return(opn.reg[addr + 0x200]);
	}
	else if (addr == 0xff) {
		return(0);
	}
	else {
		(void)port;
//		return(opn.data2);
		return(0xff);
	}
}

static REG8 IOINPCALL spr_i58c(UINT port) {

	(void)port;
	return(fmtimer.status & 3);
}

static REG8 IOINPCALL spr_i58e(UINT port) {

	UINT	addr;

	addr = opn.addr2;
	if (addr < 0x100) {
		return(opn.reg[addr + 0x200]);
	}
	else {
		(void)port;
//		return(opn.data2);
		return(0xff);
	}
}


// ----

static const IOOUT spb_o[4] = {
			spb_o188,	spb_o18a,	spb_o18c,	spb_o18e};

static const IOINP spb_i[4] = {
			spb_i188,	spb_i18a,	spb_i188,	spb_i18e};


void boardspb_reset(const NP2CFG *pConfig) {

	fmtimer_reset(pConfig->spbopt & 0xc0);
	opn.channels = 6;
	opngen_setcfg(6, OPN_STEREO | 0x03f);
	soundrom_loadex(pConfig->spbopt & 7, OEMTEXT("SPB"));
	opn.base = ((pConfig->spbopt & 0x10)?0x000:0x100);
}

void boardspb_bind(void) {

	fmboard_fmrestore(0, 0);
	fmboard_fmrestore(3, 1);
	psggen_restore(&psg1);
	fmboard_rhyrestore(&rhythm, 0);
	sound_streamregist(&opngen, (SOUNDCB)opngen_getpcmvr);
	sound_streamregist(&psg1, (SOUNDCB)psggen_getpcm);
	rhythm_bind(&rhythm);
	sound_streamregist(&adpcm, (SOUNDCB)adpcm_getpcm);
	cbuscore_attachsndex(0x188 - opn.base, spb_o, spb_i);
}


// ----

static const IOOUT spr_o[4] = {
			spr_o588,	spr_o58a,	spr_o58c,	spr_o58e};

static const IOINP spr_i[4] = {
			spr_i588,	spr_i58a,	spr_i58c,	spr_i58e};


void boardspr_reset(const NP2CFG *pConfig) {

	fmtimer_reset(pConfig->spbopt & 0xc0);
	opn.reg[0x2ff] = 0;
	opn.channels = 12;
	opngen_setcfg(12, OPN_STEREO | 0x03f);
	soundrom_loadex(pConfig->spbopt & 7, OEMTEXT("SPB"));
	opn.base = (pConfig->spbopt & 0x10)?0x000:0x100;
}

void boardspr_bind(void) {

	fmboard_fmrestore(0, 0);
	fmboard_fmrestore(3, 1);
	fmboard_fmrestore(6, 2);
	fmboard_fmrestore(9, 3);
	psggen_restore(&psg1);
	fmboard_rhyrestore(&rhythm, 0);
	sound_streamregist(&opngen, (SOUNDCB)opngen_getpcmvr);
	sound_streamregist(&psg1, (SOUNDCB)psggen_getpcm);
	rhythm_bind(&rhythm);
	sound_streamregist(&adpcm, (SOUNDCB)adpcm_getpcm);
	cbuscore_attachsndex(0x188 - opn.base, spb_o, spb_i);
	cbuscore_attachsndex(0x588 - opn.base, spr_o, spr_i);
}

