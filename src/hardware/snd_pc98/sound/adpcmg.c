#include	"compiler.h"
#include	"sound.h"
#include	"adpcm.h"


#define	ADPCM_NBR	0x80000000

static const UINT adpcmdeltatable[8] = {
		//	0.89,	0.89,	0.89,	0.89,	1.2,	1.6,	2.0,	2.4
			228,	228,	228,	228,	308,	408,	512,	612};


REG8 SOUNDCALL adpcm_readsample(ADPCM ad) {

	UINT32	pos;
	REG8	data;
	REG8	ret;

	if ((ad->reg.ctrl1 & 0x60) == 0x20) {
		pos = ad->pos & 0x1fffff;
		if (!(ad->reg.ctrl2 & 2)) {
			data = ad->buf[pos >> 3];
			pos += 8;
		}
		else {
			const UINT8 *ptr;
			REG8 bit;
			UINT tmp;
			ptr = ad->buf + ((pos >> 3) & 0x7fff);
			bit = 1 << (pos & 7);
			tmp = (ptr[0x00000] & bit);
			tmp += (ptr[0x08000] & bit) << 1;
			tmp += (ptr[0x10000] & bit) << 2;
			tmp += (ptr[0x18000] & bit) << 3;
			tmp += (ptr[0x20000] & bit) << 4;
			tmp += (ptr[0x28000] & bit) << 5;
			tmp += (ptr[0x30000] & bit) << 6;
			tmp += (ptr[0x38000] & bit) << 7;
			data = (REG8)(tmp >> (pos & 7));
			pos++;
		}
		if (pos != ad->stop) {
			pos &= 0x1fffff;
			ad->status |= 4;
		}
		if (pos >= ad->limit) {
			pos = 0;
		}
		ad->pos = pos;
	}
	else {
		data = 0;
	}
	pos = ad->fifopos;
	ret = ad->fifo[ad->fifopos];
	ad->fifo[ad->fifopos] = data;
	ad->fifopos ^= 1;
	return(ret);
}

void SOUNDCALL adpcm_datawrite(ADPCM ad, REG8 data) {

	UINT32	pos;

	pos = ad->pos & 0x1fffff;
	if (!(ad->reg.ctrl2 & 2)) {
		ad->buf[pos >> 3] = data;
		pos += 8;
	}
	else {
		UINT8 *ptr;
		UINT8 bit;
		UINT8 mask;
		ptr = ad->buf + ((pos >> 3) & 0x7fff);
		bit = 1 << (pos & 7);
		mask = ~bit;
		ptr[0x00000] &= mask;
		if (data & 0x01) {
			ptr[0x00000] |= bit;
		}
		ptr[0x08000] &= mask;
		if (data & 0x02) {
			ptr[0x08000] |= bit;
		}
		ptr[0x10000] &= mask;
		if (data & 0x04) {
			ptr[0x10000] |= bit;
		}
		ptr[0x18000] &= mask;
		if (data & 0x08) {
			ptr[0x18000] |= bit;
		}
		ptr[0x20000] &= mask;
		if (data & 0x10) {
			ptr[0x20000] |= bit;
		}
		ptr[0x28000] &= mask;
		if (data & 0x20) {
			ptr[0x28000] |= bit;
		}
		ptr[0x30000] &= mask;
		if (data & 0x40) {
			ptr[0x30000] |= bit;
		}
		ptr[0x38000] &= mask;
		if (data & 0x80) {
			ptr[0x38000] |= bit;
		}
		pos++;
	}
	if (pos == ad->stop) {
		pos &= 0x1fffff;
		ad->status |= 4;
	}
	if (pos >= ad->limit) {
		pos = 0;
	}
	ad->pos = pos;
}

static void SOUNDCALL getadpcmdata(ADPCM ad) {

	UINT32	pos;
	UINT	data;
	UINT	dir;
	SINT32	dlt;
	SINT32	samp;

	pos = ad->pos;
	if (!(ad->reg.ctrl2 & 2)) {
		data = ad->buf[(pos >> 3) & 0x3ffff];
		if (!(pos & ADPCM_NBR)) {
			data >>= 4;
		}
		pos += ADPCM_NBR + 4;
	}
	else {
		const UINT8 *ptr;
		REG8 bit;
		UINT tmp;
		ptr = ad->buf + ((pos >> 3) & 0x7fff);
		bit = 1 << (pos & 7);
		if (!(pos & ADPCM_NBR)) {
			tmp = (ptr[0x20000] & bit);
			tmp += (ptr[0x28000] & bit) << 1;
			tmp += (ptr[0x30000] & bit) << 2;
			tmp += (ptr[0x38000] & bit) << 3;
			data = tmp >> (pos & 7);
			pos += ADPCM_NBR;
		}
		else {
			tmp = (ptr[0x00000] & bit);
			tmp += (ptr[0x08000] & bit) << 1;
			tmp += (ptr[0x10000] & bit) << 2;
			tmp += (ptr[0x18000] & bit) << 3;
			data = tmp >> (pos & 7);
			pos += ADPCM_NBR + 1;
		}
	}
	dir = data & 8;
	data &= 7;
	dlt = adpcmdeltatable[data] * ad->delta;
	dlt >>= 8;
	if (dlt < 127) {
		dlt = 127;
	}
	else if (dlt > 24000) {
		dlt = 24000;
	}
	samp = ad->delta;
	ad->delta = dlt;
	samp *= ((data * 2) + 1);
	samp >>= ADPCM_SHIFT;
	if (!dir) {
		samp += ad->samp;
		if (samp > 32767) {
			samp = 32767;
		}
	}
	else {
		samp = ad->samp - samp;
		if (samp < -32767) {
			samp = -32767;
		}
	}
	ad->samp = samp;

	if (!(pos & ADPCM_NBR)) {
		if (pos == ad->stop) {
			if (ad->reg.ctrl1 & 0x10) {
				pos = ad->start;
				ad->samp = 0;
				ad->delta = 127;
			}
			else {
				pos &= 0x1fffff;
				ad->status |= 4;
				ad->play = 0;
			}
		}
		else if (pos >= ad->limit) {
			pos = 0;
		}
	}
	ad->pos = pos;
	samp *= ad->level;
	samp >>= (10 + 1);
	ad->out0 = ad->out1;
	ad->out1 = samp + ad->fb;
	ad->fb = samp >> 1;
}

void SOUNDCALL adpcm_getpcm(ADPCM ad, SINT32 *pcm, UINT count) {

	SINT32	remain;
	SINT32	samp;

	if ((count == 0) || (ad->play == 0)) {
		return;
	}
	remain = ad->remain;
	if (ad->step <= ADTIMING) {
		do {
			if (remain < 0) {
				remain += ADTIMING;
				getadpcmdata(ad);
				if (ad->play == 0) {
					if (remain > 0) {
						do {
							samp = (ad->out0 * remain) >> ADTIMING_BIT;
							if (ad->reg.ctrl2 & 0x80) {
								pcm[0] += samp;
							}
							if (ad->reg.ctrl2 & 0x40) {
								pcm[1] += samp;
							}
							pcm += 2;
							remain -= ad->step;
						} while((remain > 0) && (--count));
					}
					goto adpcmstop;
				}
			}
			samp = (ad->out0 * remain) + (ad->out1 * (ADTIMING - remain));
			samp >>= ADTIMING_BIT;
			if (ad->reg.ctrl2 & 0x80) {
				pcm[0] += samp;
			}
			if (ad->reg.ctrl2 & 0x40) {
				pcm[1] += samp;
			}
			pcm += 2;
			remain -= ad->step;
		} while(--count);
	}
	else {
		do {
			if (remain > 0) {
				samp = ad->out0 * (ADTIMING - remain);
				do {
					getadpcmdata(ad);
					if (ad->play == 0) {
						goto adpcmstop;
					}
					samp += ad->out0 * min(remain, ad->pertim);
					remain -= ad->pertim;
				} while(remain > 0);
			}
			else {
				samp = ad->out0 * ADTIMING;
			}
			remain += ADTIMING;
			samp >>= ADTIMING_BIT;
			if (ad->reg.ctrl2 & 0x80) {
				pcm[0] += samp;
			}
			if (ad->reg.ctrl2 & 0x40) {
				pcm[1] += samp;
			}
			pcm += 2;
		} while(--count);
	}
	ad->remain = remain;
	return;

adpcmstop:
	ad->out0 = 0;
	ad->out1 = 0;
	ad->fb = 0;
	ad->remain = 0;
}

