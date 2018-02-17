#include	"compiler.h"
#include	"sound.h"
#include	"tms3631.h"


extern	TMS3631CFG	tms3631cfg;


void SOUNDCALL tms3631_getpcm(TMS3631 tms, SINT32 *pcm, UINT count) {

	UINT	ch;
	SINT32	data;
	UINT	i;

	if (tms->enable == 0) {
		return;
	}
	while(count--) {
		ch = 0;
		data = 0;
		do {									// centre
			if ((tms->enable & (1 << ch)) && (tms->ch[ch].freq)) {
				for (i=0; i<4; i++) {
					tms->ch[ch].count += tms->ch[ch].freq;
					data += (tms->ch[ch].count & 0x10000)?1:-1;
				}
			}
		} while(++ch < 2);
		pcm[0] += data * tms3631cfg.left;
		pcm[1] += data * tms3631cfg.right;
		do {									// left
			if ((tms->enable & (1 << ch)) && (tms->ch[ch].freq)) {
				for (i=0; i<4; i++) {
					tms->ch[ch].count += tms->ch[ch].freq;
					pcm[0] += tms3631cfg.feet[(tms->ch[ch].count >> 16) & 15];
				}
			}
		} while(++ch < 5);
		do {									// right
			if ((tms->enable & (1 << ch)) && (tms->ch[ch].freq)) {
				for (i=0; i<4; i++) {
					tms->ch[ch].count += tms->ch[ch].freq;
					pcm[1] += tms3631cfg.feet[(tms->ch[ch].count >> 16) & 15];
				}
			}
		} while(++ch < 8);
		pcm += 2;
	}
}

