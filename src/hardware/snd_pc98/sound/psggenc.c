#include	"compiler.h"
#include	<math.h>
#include	"sound.h"
#include	"psggen.h"
#include	"keydisp.h"


	PSGGENCFG	psggencfg;

static const UINT8 psggen_deftbl[0x10] =
				{0, 0, 0, 0, 0, 0, 0, 0xbf, 0, 0, 0, 0, 0, 0, 0xff, 0xff};

static const UINT8 psgenv_pat[16] = {
					PSGENV_ONESHOT,
					PSGENV_ONESHOT,
					PSGENV_ONESHOT,
					PSGENV_ONESHOT,
					PSGENV_ONESHOT | PSGENV_INC,
					PSGENV_ONESHOT | PSGENV_INC,
					PSGENV_ONESHOT | PSGENV_INC,
					PSGENV_ONESHOT | PSGENV_INC,
					PSGENV_ONECYCLE,
					PSGENV_ONESHOT,
					0,
					PSGENV_ONESHOT | PSGENV_LASTON,
					PSGENV_ONECYCLE | PSGENV_INC,
					PSGENV_ONESHOT | PSGENV_INC | PSGENV_LASTON,
					PSGENV_INC,
					PSGENV_ONESHOT | PSGENV_INC};


void psggen_initialize(UINT rate) {

	double	pom;
	UINT	i;

	ZeroMemory(&psggencfg, sizeof(psggencfg));
	psggencfg.rate = rate;
	pom = (double)0x0c00;
	for (i=15; i; i--) {
		psggencfg.voltbl[i] = (SINT32)pom;
		pom /= 1.41492;
	}
	psggencfg.puchidec = (UINT16)(rate / 11025) * 2;
	if (psggencfg.puchidec == 0) {
		psggencfg.puchidec = 1;
	}
	if (rate) {
		psggencfg.base = (5000 * (1 << (32 - PSGFREQPADBIT - PSGADDEDBIT)))
															/ (rate / 25);
	}
}

void psggen_setvol(UINT vol) {

	UINT	i;

	for (i=1; i<16; i++) {
		psggencfg.volume[i] = (psggencfg.voltbl[i] * vol) >> 
															(6 + PSGADDEDBIT);
	}
}

void psggen_reset(PSGGEN psg) {

	UINT	i;

	ZeroMemory(psg, sizeof(_PSGGEN));
	for (i=0; i<3; i++) {
		psg->tone[i].pvol = psggencfg.volume + 0;
	}
	for (i=0; i<sizeof(psggen_deftbl); i++) {
		psggen_setreg(psg, i, psggen_deftbl[i]);
	}
}

void psggen_restore(PSGGEN psg) {

	UINT	i;

	for (i=0; i<0x0e; i++) {
		psggen_setreg(psg, i, ((UINT8 *)&psg->reg)[i]);
	}
}

void psggen_setreg(PSGGEN psg, UINT reg, REG8 value) {

	UINT	ch;
	UINT	freq;

	reg = reg & 15;
	if (reg < 14) {
		sound_sync();
	}
	((UINT8 *)&psg->reg)[reg] = value;
	switch(reg) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			ch = reg >> 1;
			freq = LOADINTELWORD(psg->reg.tune[ch]) & 0xfff;
			if (freq > 9) {
				psg->tone[ch].freq = (psggencfg.base / freq) << PSGFREQPADBIT;
			}
			else {
				psg->tone[ch].freq = 0;
			}
			break;

		case 6:
			freq = value & 0x1f;
			if (freq == 0) {
				freq = 1;
			}
			psg->noise.freq = psggencfg.base / freq;
			psg->noise.freq <<= PSGFREQPADBIT;
			break;

		case 7:
			keydisp_psgmix(psg);
			psg->mixer = ~value;
			psg->puchicount = psggencfg.puchidec;
//			TRACEOUT(("psg %x 7 %d", (long)psg, value));
			break;

		case 8:
		case 9:
		case 10:
			ch = reg - 8;
			keydisp_psgvol(psg, (UINT8)ch);
			if (value & 0x10) {
				psg->tone[ch].pvol = &psg->evol;
			}
			else {
				psg->tone[ch].pvol = psggencfg.volume + (value & 15);
			}
			psg->tone[ch].puchi = psggencfg.puchidec;
			psg->puchicount = psggencfg.puchidec;
//			TRACEOUT(("psg %x %x %d", (long)psg, reg, value));
			break;

		case 11:
		case 12:
			freq = LOADINTELWORD(psg->reg.envtime);
			freq = psggencfg.rate * freq / 125000;
			if (freq == 0) {
				freq = 1;
			}
			psg->envmax = freq;
			break;

		case 13:
			psg->envmode = psgenv_pat[value & 0x0f];
			psg->envvolcnt = 16;
			psg->envcnt = 1;
			break;
	}
}

REG8 psggen_getreg(PSGGEN psg, UINT reg) {

	return(((UINT8 *)&psg->reg)[reg & 15]);
}

void psggen_setpan(PSGGEN psg, UINT ch, REG8 pan) {

	if ((psg) && (ch < 3)) {
		psg->tone[ch].pan = pan;
	}
}

