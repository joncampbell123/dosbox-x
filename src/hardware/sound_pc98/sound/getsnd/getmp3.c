#include	"compiler.h"
#include	"getsnd.h"


#if defined(SUPPORT_MP3)
#include	"amethyst.h"

#define mp3_getdecver			__mp3_getdecver
#define mp3_create				__mp3_create
#define mp3_destroy				__mp3_destroy
#define mp3_predecode			__mp3_predecode
#define mp3_decode				__mp3_decode
#define mp3_adjustgain			__mp3_adjustgain


static UINT mp3_dec(GETSND snd, short *dst) {

	UINT8	*src;
	MPEGL3	*mp3;
	int		r;

	src = snd->datptr;
	mp3 = (MPEGL3 *)snd->snd;
	if (snd->datsize < 4) {
		goto mp3dec_err;
	}
	r = mp3_predecode(mp3, src);
	if (r) {
		if ((r != MPEGHEAD_RENEWAL) ||
			(snd->samplingrate != mp3->c.samplingrate) ||
			(snd->channels != mp3->c.channels)) {
			TRACEOUT(("mp3 decord err"));
			goto mp3dec_err;
		}
	}

	if (snd->datsize < mp3->c.insize) {
		goto mp3dec_err;
	}
	snd->datptr += mp3->c.insize;
	snd->datsize -= mp3->c.insize;
	mp3_decode(mp3, dst, src, mp3->c.insize);
	return(mp3->c.outsamples);

mp3dec_err:
	return(0);
}

static void mp3_decend(GETSND snd) {

	mp3_destroy((MPEGL3 *)snd->snd);
}

BOOL __mp3_open(GETSND snd, UINT8 *ptr, UINT size) {

	MPEGL3	*mp3;

	if (size < 4) {
		goto mp3opn_err;
	}
	mp3 = mp3_create(ptr);
	if (mp3 == NULL) {
		goto mp3opn_err;
	}
	snd->datptr = ptr;
	snd->datsize = size;

	snd->snd = mp3;
	snd->dec = (GSDEC)mp3_dec;
	snd->decend = mp3_decend;

	snd->samplingrate = mp3->c.samplingrate;
	snd->channels = mp3->c.channels;
	snd->blocksize = 1728;
	snd->blocksamples = mp3->c.outsamples;
	snd->bit = 16;
	TRACEOUT(("mp3: %dHz %dkbps", mp3->c.samplingrate, mp3->c.kbitrate));
	return(SUCCESS);

mp3opn_err:
	return(FAILURE);
}

BOOL getmp3_open(GETSND snd, UINT8 *ptr, UINT size) {

	UINT	pos;

	if ((size < 10) && (!memcmp(ptr, "ID3", 3))) {
		pos = (ptr[6] & 0x7f);
		pos <<= 7;
		pos |= (ptr[7] & 0x7f);
		pos <<= 7;
		pos |= (ptr[8] & 0x7f);
		pos <<= 7;
		pos |= (ptr[9] & 0x7f);
		pos += 10;
		TRACEOUT(("ID3 Tag - size:%dbyte(s)", pos));
		if (size < pos) {
			goto mp3opn_err;
		}
		ptr += pos;
		size -= pos;
	}
	if (__mp3_open(snd, ptr, size) != SUCCESS) {
		goto mp3opn_err;
	}
	return(SUCCESS);

mp3opn_err:
	return(FAILURE);
}
#endif

