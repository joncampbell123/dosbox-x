#include	"compiler.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"cbuscore.h"
#include	"board86.h"
#include	"pcm86io.h"
#include	"sound.h"
#include	"fmboard.h"
#include	"s98.h"


static void IOOUTCALL opna_o188(UINT port, REG8 dat) {

	opn.addr = dat;
	opn.data = dat;
	(void)port;
}

static void IOOUTCALL opna_o18a(UINT port, REG8 dat) {

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
	(void)port;
}

static void IOOUTCALL opna_o18c(UINT port, REG8 dat) {

	if (opn.extend) {
		opn.addr = dat + 0x100;
		opn.data = dat;
	}
	(void)port;
}

static void IOOUTCALL opna_o18e(UINT port, REG8 dat) {

	UINT	addr;

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

static REG8 IOINPCALL opna_i188(UINT port) {

	(void)port;
	return(fmtimer.status);
}

static REG8 IOINPCALL opna_i18a(UINT port) {

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
	(void)port;
	return(opn.data);
}

static REG8 IOINPCALL opna_i18c(UINT port) {

	if (opn.extend) {
		return((fmtimer.status & 3) | (opn.adpcmmask & 8));
	}
	(void)port;
	return(0xff);
}

static REG8 IOINPCALL opna_i18e(UINT port) {

	if (opn.extend) {
		UINT addr = opn.addr - 0x100;
		if ((addr == 0x08) || (addr == 0x0f)) {
			return(opn.reg[addr + 0x100]);
		}
		return(opn.data);
	}
	(void)port;
	return(0xff);
}

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


// ----

static const IOOUT opna_o[4] = {
			opna_o188,	opna_o18a,	opna_o18c,	opna_o18e};

static const IOINP opna_i[4] = {
			opna_i188,	opna_i18a,	opna_i18c,	opna_i18e};


void board86_reset(const NP2CFG *pConfig) {

	fmtimer_reset((pConfig->snd86opt & 0x10) |
					((pConfig->snd86opt & 0x4) << 5) |
					((pConfig->snd86opt & 0x8) << 3));
	opngen_setcfg(3, OPN_STEREO | 0x038);
	if (pConfig->snd86opt & 2) {
		soundrom_load(0xcc000, OEMTEXT("86"));
	}
	opn.base = (pConfig->snd86opt & 0x01)?0x000:0x100;
	fmboard_extreg(extendchannel);
}

void board86_bind(void) {

	fmboard_fmrestore(0, 0);
	fmboard_fmrestore(3, 1);
	psggen_restore(&psg1);
	fmboard_rhyrestore(&rhythm, 0);
	sound_streamregist(&opngen, (SOUNDCB)opngen_getpcm);
	sound_streamregist(&psg1, (SOUNDCB)psggen_getpcm);
	rhythm_bind(&rhythm);
	pcm86io_bind();
	cbuscore_attachsndex(0x188 + opn.base, opna_o, opna_i);
}


// ---- + chibioto

static void IOOUTCALL opnac_o18e(UINT port, REG8 dat) {

	UINT	addr;

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
		if (addr < 0x12) {
			adpcm_setreg(&adpcm, addr, dat);
		}
	}
	(void)port;
}

static REG8 IOINPCALL opnac_i18c(UINT port) {

	if (opn.extend) {
		return((fmtimer.status & 3) | adpcm_status(&adpcm));
//		return((fmtimer.status & 3) | (opn.adpcmmask & 8));
	}
	(void)port;
	return(0xff);
}

static REG8 IOINPCALL opnac_i18e(UINT port) {

	if (opn.extend) {
		UINT addr = opn.addr - 0x100;
		if (addr == 0x08) {
			return(adpcm_readsample(&adpcm));
		}
		else if (addr == 0x0f) {
			return(opn.reg[addr + 0x100]);
		}
		return(opn.data);
	}
	(void)port;
	return(0xff);
}

static const IOOUT opnac_o[4] = {
			opna_o188,	opna_o18a,	opna_o18c,	opnac_o18e};

static const IOINP opnac_i[4] = {
			opna_i188,	opna_i18a,	opnac_i18c,	opnac_i18e};


void board86c_bind(void) {

	fmboard_fmrestore(0, 0);
	fmboard_fmrestore(3, 1);
	psggen_restore(&psg1);
	fmboard_rhyrestore(&rhythm, 0);
	sound_streamregist(&opngen, (SOUNDCB)opngen_getpcm);
	sound_streamregist(&psg1, (SOUNDCB)psggen_getpcm);
	rhythm_bind(&rhythm);
	sound_streamregist(&adpcm, (SOUNDCB)adpcm_getpcm);
	pcm86io_bind();
	cbuscore_attachsndex(0x188 + opn.base, opnac_o, opnac_i);
}

