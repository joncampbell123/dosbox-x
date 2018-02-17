#include	"compiler.h"
#include	"sound.h"
#include	"opngen.h"
#include	"adpcm.h"


	ADPCMCFG	adpcmcfg;

void adpcm_initialize(UINT rate) {

	adpcmcfg.rate = rate;
}

void adpcm_setvol(UINT vol) {

	adpcmcfg.vol = vol;
}

void adpcm_reset(ADPCM ad) {

	ZeroMemory(ad, sizeof(_ADPCM));
	ad->mask = 0;					// (UINT8)~0x1c;
	ad->delta = 127;
	STOREINTELWORD(ad->reg.stop, 0x0002);
	STOREINTELWORD(ad->reg.limit, 0xffff);
	ad->stop = 0x000060;
	ad->limit = 0x200000;
	adpcm_update(ad);
}

void adpcm_update(ADPCM ad) {

	UINT32	addr;

	if (adpcmcfg.rate) {
		ad->base = ADTIMING * (OPNA_CLOCK / 72) / adpcmcfg.rate;
	}
	addr = LOADINTELWORD(ad->reg.delta);
	addr = (addr * ad->base) >> 16;
	if (addr < 0x80) {
		addr = 0x80;
	}
	ad->step = addr;
	ad->pertim = (1 << (ADTIMING_BIT * 2)) / addr;
	ad->level = (ad->reg.level * adpcmcfg.vol) >> 4;
}

void adpcm_setreg(ADPCM ad, UINT reg, REG8 value) {

	UINT32	addr;

	sound_sync();
	((UINT8 *)(&ad->reg))[reg] = value;
	switch(reg) {
		case 0x00:								// control1
			if ((value & 0x80) && (!ad->play)) {
				ad->play = 0x20;
				ad->pos = ad->start;
				ad->samp = 0;
				ad->delta = 127;
				ad->remain = 0;
			}
			if (value & 1) {
				ad->play = 0;
			}
			break;

		case 0x01:								// control2
			break;

		case 0x02:	case 0x03:					// start address
			addr = (LOADINTELWORD(ad->reg.start)) << 5;
			ad->pos = addr;
			ad->start = addr;
			break;

		case 0x04:	case 0x05:					// stop address
			addr = (LOADINTELWORD(ad->reg.stop) + 1) << 5;
			ad->stop = addr;
			break;

		case 0x08:								// data
			if ((ad->reg.ctrl1 & 0x60) == 0x60) {
				adpcm_datawrite(ad, value);
			}
			break;

		case 0x09:	case 0x0a:					// delta
			addr = LOADINTELWORD(ad->reg.delta);
			addr = (addr * ad->base) >> 16;
			if (addr < 0x80) {
				addr = 0x80;
			}
			ad->step = addr;
			ad->pertim = (1 << (ADTIMING_BIT * 2)) / addr;
			break;

		case 0x0b:								// level
			ad->level = (value * adpcmcfg.vol) >> 4;
			break;

		case 0x0c:	case 0x0d:					// limit address
			addr = (LOADINTELWORD(ad->reg.limit) + 1) << 5;
			ad->limit = addr;
			break;

		case 0x10:								// flag
			if (value & 0x80) {
				ad->status = 0;
			}
			else {
				ad->mask = ~(value & 0x1f);
			}
			break;
	}
}

REG8 adpcm_status(ADPCM ad) {

	return(((ad->status | 8) & ad->mask) | ad->play);
}

