#include	"compiler.h"
#include	"sound.h"
#include	"tms3631.h"


	TMS3631CFG	tms3631cfg;

static const UINT16 tms3631_freqtbl[] = {
			0,	0x051B, 0x0569, 0x05BB, 0x0613, 0x066F, 0x06D1,
				0x0739, 0x07A7, 0x081B, 0x0897, 0x091A, 0x09A4, 0,0,0,
			0,	0x0A37, 0x0AD3, 0x0B77, 0x0C26, 0x0CDF, 0x0DA3,
				0x0E72, 0x0F4E, 0x1037, 0x112E, 0x1234, 0x1349, 0,0,0,
			0,	0x146E, 0x15A6, 0x16EF, 0x184C, 0x19BE, 0x1B46,
				0x1CE5, 0x1E9D, 0x206F, 0x225D, 0x2468, 0x2692, 0,0,0,
			0,	0x28DD, 0x2B4C, 0x2DDF, 0x3099, 0x337D, 0x368D,
				0x39CB, 0x3D3B, 0x40DF, 0x44BA, 0x48D1, 0x4D25, 0x51BB, 0,0};


void tms3631_initialize(UINT rate) {

	UINT	sft;

	ZeroMemory(&tms3631cfg, sizeof(tms3631cfg));
	sft = 0;
	if (rate == 11025) {
		sft = 0;
	}
	else if (rate == 22050) {
		sft = 1;
	}
	else if (rate == 44100) {
		sft = 2;
	}
	tms3631cfg.ratesft = sft;
}

void tms3631_setvol(const UINT8 *vol) {

	UINT	i;
	UINT	j;
	SINT32	data;

	tms3631cfg.left = (vol[0] & 15) << 5;
	tms3631cfg.right = (vol[1] & 15) << 5;
	vol += 2;
	for (i=0; i<16; i++) {
		data = 0;
		for (j=0; j<4; j++) {
			data += (vol[j] & 15) * ((i & (1 << j))?1:-1);
		}
		tms3631cfg.feet[i] = data << 5;
	}
}


// ----

void tms3631_reset(TMS3631 tms) {

	ZeroMemory(tms, sizeof(_TMS3631));
}

void tms3631_setkey(TMS3631 tms, REG8 ch, REG8 key) {

	tms->ch[ch & 7].freq = tms3631_freqtbl[key & 0x3f] >> tms3631cfg.ratesft;
}

void tms3631_setenable(TMS3631 tms, REG8 enable) {

	tms->enable = enable;
}

