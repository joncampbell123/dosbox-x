#include	"compiler.h"
#include	"getsnd.h"


GETSND getsnd_create(void *datptr, UINT datsize) {

	_GETSND		snd;
	BOOL		r;
	UINT		size;
	UINT		blkwork;
	GETSND		ret;

	ZeroMemory(&snd, sizeof(snd));
	r = getwave_open(&snd, (UINT8 *)datptr, datsize);
#if defined(SUPPORT_MP3)
	if (r == FAILURE) {
		r = getmp3_open(&snd, (UINT8 *)datptr, datsize);
	}
#endif
#if defined(SUPPORT_OGG)
	if (r == FAILURE) {
		r = getogg_open(&snd, (UINT8 *)datptr, datsize);
	}
#endif
	if (r == FAILURE) {
		goto gscre_err0;
	}

	blkwork = (snd.bit + 7) >> 3;
	blkwork *= snd.channels;
	blkwork *= snd.blocksamples;
	size = blkwork + snd.blocksize;

	ret = (GETSND)_MALLOC(sizeof(_GETSND) + size, "GETSND");
	if (ret == NULL) {
		goto gscre_err1;
	}
	ZeroMemory(ret + 1, size);

	// ƒ[ƒN‚Æ‚©Ý’èB
	snd.buffer = (UINT8 *)(ret + 1);
	snd.work = snd.buffer + blkwork;
	*ret = snd;
	if (getsnd_setmixproc(ret, snd.samplingrate, snd.channels) != SUCCESS) {
		TRACEOUT(("err"));
		goto gscre_err1;
	}
	return(ret);

gscre_err1:
	if (snd.decend) {
		(*snd.decend)(&snd);
	}

gscre_err0:
	return(NULL);
}


void getsnd_destroy(GETSND snd) {

	if (snd == NULL) {
		goto gsdes_end;
	}
	if (snd->decend) {
		(*snd->decend)(snd);
	}
	_MFREE(snd);

gsdes_end:
	return;
}

UINT getsnd_getpcmbyleng(GETSND snd, void *pcm, UINT leng) {

	UINT8	*pcmp;
	UINT8	*pcmterm;

	if (snd == NULL) {
		goto gsgpl_err;
	}

	pcmp = (UINT8 *)pcm;
	pcmterm = pcmp + leng;
	while(pcmp < pcmterm) {
		if (snd->remain != 0) {
			pcmp = (UINT8 *)(*snd->cnv)(snd, pcmp, pcmterm);
		}
		if (snd->remain == 0) {
			snd->buf = snd->buffer;
			snd->remain = (*snd->dec)(snd, snd->buffer);
			if (snd->remain == 0) {
				break;
			}
		}
	}
	return((UINT)(pcmp - (UINT8 *)pcm));

gsgpl_err:
	return(0);
}

