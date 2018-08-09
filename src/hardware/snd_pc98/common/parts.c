#include    "np2glue.h"

//#include	"compiler.h"
#include	"parts.h"


static	SINT32	randseed = 1;


void PARTSCALL rand_setseed(SINT32 seed) {

	randseed = seed;
}

SINT32 PARTSCALL rand_get(void) {

	randseed = (randseed * 0x343fd) + 0x269ec3;
	return(randseed >> 16);
}

UINT8 PARTSCALL AdjustAfterMultiply(UINT8 value) {

	return((UINT8)(((value / 10) << 4) + (value % 10)));
}

UINT8 PARTSCALL AdjustBeforeDivision(UINT8 value) {

	return((UINT8)(((value >> 4) * 10) + (value & 0xf)));
}

UINT PARTSCALL sjis2jis(UINT sjis) {

	UINT	ret;

	ret = sjis & 0xff;
	ret -= (ret >> 7);
	ret += 0x62;
	if (ret < 256) {
		ret = (ret - 0xa2) & 0x1ff;
	}
	ret += 0x1f21;
	ret += (sjis & 0x3f00) << 1;
	return(ret);
}

UINT PARTSCALL jis2sjis(UINT jis) {

	UINT	high;
	UINT	low;

	low = jis & 0x7f;
	high = (jis >> 8) & 0x7f;
	low += ((high & 1) - 1) & 0x5e;
	if (low >= 0x60) {
		low++;
	}
	high += 0x121;
	low += 0x1f;
	high >>= 1;
	high ^= 0x20;
	return((high << 8) | low);
}

void PARTSCALL satuation_s16(SINT16 *dst, const SINT32 *src, UINT size) {

	SINT32	data;

	size >>= 1;
	while(size--) {
		data = *src++;
		if (data > 32767) {
			data = 32767;
		}
		else if (data < -32768) {
			data = -32768;
		}
		*dst++ = (SINT16)data;
	}
}

void PARTSCALL satuation_s16x(SINT16 *dst, const SINT32 *src, UINT size) {

	SINT32	data;

	size >>= 2;
	while(size--) {
		data = src[0];
		if (data > 32767) {
			data = 32767;
		}
		else if (data < -32768) {
			data = -32768;
		}
		dst[1] = (SINT16)data;
		data = src[1];
		if (data > 32767) {
			data = 32767;
		}
		else if (data < -32768) {
			data = -32768;
		}
		dst[0] = (SINT16)data;
		src += 2;
		dst += 2;
	}
}

