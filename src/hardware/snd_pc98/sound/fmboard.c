#include	"compiler.h"
#include	"joymng.h"
#include	"soundmng.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"cbuscore.h"
#include	"board14.h"
#include	"board26k.h"
#include	"board86.h"
#include	"boardx2.h"
#include	"board118.h"
#include	"boardspb.h"
#if defined(SUPPORT_PX)
#include	"boardpx.h"
#endif	// defined(SUPPORT_PX)
#include	"amd98.h"
#include	"pcm86io.h"
#include	"cs4231io.h"
#include	"sound.h"
#include	"fmboard.h"
#include	"beep.h"
#include	"keydisp.h"
#include	"keystat.h"


	UINT32		usesound;
	OPN_T		opn;
	AMD98		amd98;
	MUSICGEN	musicgen;

	_TMS3631	tms3631;
	_FMTIMER	fmtimer;
	_OPNGEN		opngen;
	OPNCH		opnch[OPNCH_MAX];
	_PSGGEN		__psg[3];
	_RHYTHM		rhythm;
	_ADPCM		adpcm;
	_PCM86		pcm86;
	_CS4231		cs4231;

#if defined(SUPPORT_PX)
	OPN_T		opn2;
	OPN_T		opn3;
	_RHYTHM		rhythm2;
	_RHYTHM		rhythm3;
	_ADPCM		adpcm2;
	_ADPCM		adpcm3;
#endif	// defined(SUPPORT_PX)


static void	(*extfn)(REG8 enable);


// ----

static	REG8	rapids = 0;

REG8 fmboard_getjoy(PSGGEN psg) {

	REG8	ret;

	rapids ^= 0xf0;											// ver0.28
	ret = 0xff;
	if (!(psg->reg.io2 & 0x40)) {
		ret &= (joymng_getstat() | (rapids & 0x30));
		if (np2cfg.KEY_MODE == 1) {
			ret &= keystat_getjoy();
		}
	}
	else {
		if (np2cfg.KEY_MODE == 2) {
			ret &= keystat_getjoy();
		}
	}
	if (np2cfg.BTN_RAPID) {
		ret |= rapids;
	}

	// rapid‚Æ”ñrapid‚ð‡¬								// ver0.28
	ret &= ((ret >> 2) | (~0x30));

	if (np2cfg.BTN_MODE) {
		UINT8 bit1 = (ret & 0x20) >> 1;					// ver0.28
		UINT8 bit2 = (ret & 0x10) << 1;
		ret = (ret & (~0x30)) | bit1 | bit2;
	}

	// intr ”½‰f‚µ‚ÄI‚í‚è								// ver0.28
	ret &= 0x3f;
	ret |= fmtimer.intr;
	return(ret);
}


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

void fmboard_reset(const NP2CFG *pConfig, UINT32 type) {

	UINT8	cross;

	soundrom_reset();
	beep_reset();												// ver0.27a
	cross = np2cfg.snd_x;										// ver0.30

	extfn = NULL;
	ZeroMemory(&opn, sizeof(opn));
	setfmregs(opn.reg + 0x000);
	setfmregs(opn.reg + 0x100);
	setfmregs(opn.reg + 0x200);
	setfmregs(opn.reg + 0x300);
	opn.reg[0xff] = 0x01;
	opn.channels = 3;
	opn.adpcmmask = (UINT8)~(0x1c);

#if defined(SUPPORT_PX)
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
#endif	// defined(SUPPORT_PX)

	ZeroMemory(&musicgen, sizeof(musicgen));
	ZeroMemory(&amd98, sizeof(amd98));

	tms3631_reset(&tms3631);
	opngen_reset();
	psggen_reset(&psg1);
	psggen_reset(&psg2);
	psggen_reset(&psg3);
	rhythm_reset(&rhythm);
#if defined(SUPPORT_PX)
	rhythm_reset(&rhythm2);
	rhythm_reset(&rhythm3);
#endif	// defined(SUPPORT_PX)
	adpcm_reset(&adpcm);
#if defined(SUPPORT_PX)
	adpcm_reset(&adpcm2);
	adpcm_reset(&adpcm3);
#endif	// defined(SUPPORT_PX)
	pcm86_reset();
	cs4231_reset();

	switch(type) {
		case 0x01:
			board14_reset(pConfig);
			break;

		case 0x02:
			board26k_reset(pConfig);
			break;

		case 0x04:
			board86_reset(pConfig);
			break;

		case 0x06:
			boardx2_reset(pConfig);
			break;

		case 0x08:
			board118_reset(pConfig);
			break;

		case 0x14:
			board86_reset(pConfig);
			break;

		case 0x20:
			boardspb_reset(pConfig);
			cross ^= pConfig->spb_x;
			break;

		case 0x40:
			boardspr_reset(pConfig);
			cross ^= pConfig->spb_x;
			break;

		case 0x80:
//			amd98_reset(pConfig);
			break;

#if	defined(SUPPORT_PX)
		case 0x30:
			boardpx1_reset(pConfig);
			break;

		case 0x50:
			boardpx2_reset(pConfig);
			break;
#endif	// defined(SUPPORT_PX)

		default:
			type = 0;
			break;
	}
	usesound = type;
	soundmng_setreverse(cross);
	keydisp_setfmboard(type);
	opngen_setVR(pConfig->spb_vrc, pConfig->spb_vrl);
}

void fmboard_bind(void) {

	switch(usesound) {
		case 0x01:
			board14_bind();
			break;

		case 0x02:
			board26k_bind();
			break;

		case 0x04:
			board86_bind();
			break;

		case 0x06:
			boardx2_bind();
			break;

		case 0x08:
			board118_bind();
			break;

		case 0x14:
			board86c_bind();
			break;

		case 0x20:
			boardspb_bind();
			break;

		case 0x40:
			boardspr_bind();
			break;

		case 0x80:
			amd98_bind();
			break;

#if defined(SUPPORT_PX)
		case 0x30:
			boardpx1_bind();
			break;

		case 0x50:
			boardpx2_bind();
			break;
#endif	// defined(SUPPORT_PX)
	}
	sound_streamregist(&beep, (SOUNDCB)beep_getpcm);
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


#if defined(SUPPORT_PX)
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
#endif	// defined(SUPPORT_PX)

