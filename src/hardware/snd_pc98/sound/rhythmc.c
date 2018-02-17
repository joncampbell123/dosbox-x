#include	"compiler.h"
#include	<math.h>
#include	"pccore.h"
#include	"sound.h"
#include	"rhythm.h"


static const OEMCHAR file_2608bd[] = OEMTEXT("2608_bd.wav");
static const OEMCHAR file_2608sd[] = OEMTEXT("2608_sd.wav");
static const OEMCHAR file_2608top[] = OEMTEXT("2608_top.wav");
static const OEMCHAR file_2608hh[] = OEMTEXT("2608_hh.wav");
static const OEMCHAR file_2608tom[] = OEMTEXT("2608_tom.wav");
static const OEMCHAR file_2608rim[] = OEMTEXT("2608_rim.wav");

static const OEMCHAR *rhythmfile[6] = {
				file_2608bd,	file_2608sd,	file_2608top,
				file_2608hh,	file_2608tom,	file_2608rim};

typedef struct {
	UINT	rate;
	UINT	pcmexist;
	PMIXDAT	pcm[6];
	UINT	vol;
	UINT	voltbl[96];
} RHYTHMCFG;

static	RHYTHMCFG	rhythmcfg;


void rhythm_initialize(UINT rate) {

	UINT	i;

	ZeroMemory(&rhythmcfg, sizeof(rhythmcfg));
	rhythmcfg.rate = rate;

	for (i=0; i<96; i++) {
		rhythmcfg.voltbl[i] = (UINT)(32768.0 *
										pow(2.0, (double)i * (-3.0) / 40.0));
	}
}

void rhythm_deinitialize(void) {

	UINT	i;
	SINT16	*ptr;

	for (i=0; i<6; i++) {
		ptr = rhythmcfg.pcm[i].sample;
		rhythmcfg.pcm[i].sample = NULL;
		if (ptr) {
			_MFREE(ptr);
		}
	}
}

static void rhythm_load(void) {

	int		i;
	OEMCHAR	path[MAX_PATH];

	for (i=0; i<6; i++) {
		if (rhythmcfg.pcm[i].sample == NULL) {
			getbiospath(path, rhythmfile[i], NELEMENTS(path));
			pcmmix_regfile(rhythmcfg.pcm + i, path, rhythmcfg.rate);
		}
	}
}

UINT rhythm_getcaps(void) {

	UINT	ret;
	UINT	i;

	ret = 0;
	for (i=0; i<6; i++) {
		if (rhythmcfg.pcm[i].sample) {
			ret |= 1 << i;
		}
	}
	return(ret);
}

void rhythm_setvol(UINT vol) {

	rhythmcfg.vol = vol;
}

void rhythm_reset(RHYTHM rhy) {

	ZeroMemory(rhy, sizeof(_RHYTHM));
}

void rhythm_bind(RHYTHM rhy) {

	UINT	i;

	rhythm_load();
	rhy->hdr.enable = 0x3f;
	for (i=0; i<6; i++) {
		rhy->trk[i].data = rhythmcfg.pcm[i];
	}
	rhythm_update(rhy);
	sound_streamregist(rhy, (SOUNDCB)pcmmix_getpcm);
}

void rhythm_update(RHYTHM rhy) {

	UINT	i;

	for (i=0; i<6; i++) {
		rhy->trk[i].volume = (rhythmcfg.voltbl[rhy->vol + rhy->trkvol[i]] *
														rhythmcfg.vol) >> 10;
	}
}

void rhythm_setreg(RHYTHM rhy, UINT reg, REG8 value) {

	PMIXTRK	*trk;
	REG8	bit;

	if (reg == 0x10) {
		sound_sync();
		trk = rhy->trk;
		bit = 0x01;
		do {
			if (value & bit) {
				if (value & 0x80) {
					rhy->hdr.playing &= ~((UINT)bit);
				}
				else if (trk->data.sample) {
					trk->pcm = trk->data.sample;
					trk->remain = trk->data.samples;
					rhy->hdr.playing |= bit;
				}
			}
			trk++;
			bit <<= 1;
		} while(bit < 0x40);
	}
	else if (reg == 0x11) {
		sound_sync();
		rhy->vol = (~value) & 0x3f;
		rhythm_update(rhy);
	}
	else if ((reg >= 0x18) && (reg < 0x1e)) {
		sound_sync();
		trk = rhy->trk + (reg - 0x18);
		trk->flag = ((value & 0x80) >> 7) + ((value & 0x40) >> 5);
		value = (~value) & 0x1f;
		rhy->trkvol[reg - 0x18] = (UINT8)value;
		trk->volume = (rhythmcfg.voltbl[rhy->vol + value] *
														rhythmcfg.vol) >> 10;
	}
}

