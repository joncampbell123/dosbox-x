#include	"compiler.h"
#include	"wavefile.h"
#include	"dosio.h"
#include	"soundmng.h"
#include	"cpucore.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"sound.h"
#include	"sndcsec.h"
#include	"beep.h"
#include	"getsnd.h"


	SOUNDCFG	soundcfg;


#define	STREAM_CBMAX	16

typedef struct {
	void	*hdl;
	SOUNDCB	cbfn;
} CBTBL;

typedef struct {
	SINT32	*buffer;
	SINT32	*ptr;
	UINT	samples;
	UINT	reserve;
	UINT	remain;
#if defined(SUPPORT_WAVEREC)
	WAVEWR	rec;
#endif
	CBTBL	*cbreg;
	CBTBL	cb[STREAM_CBMAX];
} SNDSTREAM;

static	SNDSTREAM	sndstream;


static void streamreset(void) {

	SNDCSEC_ENTER;
	sndstream.ptr = sndstream.buffer;
	sndstream.remain = sndstream.samples + sndstream.reserve;
	sndstream.cbreg = sndstream.cb;
	SNDCSEC_LEAVE;
}

static void streamprepare(UINT samples) {

	CBTBL	*cb;
	UINT	count;

	count = min(sndstream.remain, samples);
	if (count) {
		ZeroMemory(sndstream.ptr, count * 2 * sizeof(SINT32));
		cb = sndstream.cb;
		while(cb < sndstream.cbreg) {
			cb->cbfn(cb->hdl, sndstream.ptr, count);
			cb++;
		}
		sndstream.ptr += count * 2;
		sndstream.remain -= count;
	}
}


#if defined(SUPPORT_WAVEREC)
// ---- wave rec

BOOL sound_recstart(const OEMCHAR *filename) {

	WAVEWR	rec;

	sound_recstop();
	if (sndstream.buffer == NULL) {
		return(FAILURE);
	}
	rec = wavewr_open(filename, soundcfg.rate, 16, 2);
	sndstream.rec = rec;
	if (rec) {
		return(SUCCESS);
	}
	return(FAILURE);
}

void sound_recstop(void) {

	WAVEWR	rec;

	rec = sndstream.rec;
	sndstream.rec = NULL;
	wavewr_close(rec);
}

static void streamfilewrite(UINT samples) {

	CBTBL	*cb;
	UINT	count;
	SINT32	buf32[2*512];
	UINT8	buf[2*2*512];
	UINT	r;
	UINT	i;
	SINT32	samp;

	while(samples) {
		count = min(samples, 512);
		ZeroMemory(buf32, count * 2 * sizeof(SINT32));
		cb = sndstream.cb;
		while(cb < sndstream.cbreg) {
			cb->cbfn(cb->hdl, buf32, count);
			cb++;
		}
		r = min(sndstream.remain, count);
		if (r) {
			CopyMemory(sndstream.ptr, buf32, r * 2 * sizeof(SINT32));
			sndstream.ptr += r * 2;
			sndstream.remain -= r;
		}
		for (i=0; i<count*2; i++) {
			samp = buf32[i];
			if (samp > 32767) {
				samp = 32767;
			}
			else if (samp < -32768) {
				samp = -32768;
			}
			// little endianなので satuation_s16は使えない
			buf[i*2+0] = (UINT8)samp;
			buf[i*2+1] = (UINT8)(samp >> 8);
		}
		wavewr_write(sndstream.rec, buf, count * 4);
		samples -= count;
	}
}

static void filltailsample(UINT count) {

	SINT32	*ptr;
	UINT	orgsize;
	SINT32	sampl;
	SINT32	sampr;

	count = min(sndstream.remain, count);
	if (count) {
		ptr = sndstream.ptr;
		orgsize = (ptr - sndstream.buffer) / 2;
		if (orgsize == 0) {
			sampl = 0;
			sampr = 0;
		}
		else {
			sampl = *(ptr - 2);
			sampr = *(ptr - 1);
		}
		sndstream.ptr += count * 2;
		sndstream.remain -= count;
		do {
			ptr[0] = sampl;
			ptr[1] = sampr;
			ptr += 2;
		} while(--count);
	}
}
#endif


// ----

BOOL sound_create(UINT rate, UINT ms) {

	UINT	samples;
	UINT	reserve;

	ZeroMemory(&sndstream, sizeof(sndstream));
	switch(rate) {
		case 11025:
		case 22050:
		case 44100:
			break;

		default:
			return(FAILURE);
	}
	samples = soundmng_create(rate, ms);
	if (samples == 0) {
		goto scre_err1;
	}
	soundmng_reset();

	soundcfg.rate = rate;
	sound_changeclock();

#if defined(SOUNDRESERVE)
	reserve = rate * SOUNDRESERVE / 1000;
#else
	reserve = 0;
#endif
	sndstream.buffer = (SINT32 *)_MALLOC((samples + reserve) * 2 
												* sizeof(SINT32), "stream");
	if (sndstream.buffer == NULL) {
		goto scre_err2;
	}
	sndstream.samples = samples;
	sndstream.reserve = reserve;

	SNDCSEC_INIT;
	streamreset();
	return(SUCCESS);

scre_err2:
	soundmng_destroy();

scre_err1:
	return(FAILURE);
}

void sound_destroy(void) {

	if (sndstream.buffer) {
#if defined(SUPPORT_WAVEREC)
		sound_recstop();
#endif
		soundmng_stop();
		streamreset();
		soundmng_destroy();
		SNDCSEC_TERM;
		_MFREE(sndstream.buffer);
		sndstream.buffer = NULL;
	}
}

void sound_reset(void) {

	if (sndstream.buffer) {
		soundmng_reset();
		streamreset();
		soundcfg.lastclock = CPU_CLOCK;
		beep_eventreset();
	}
}

void sound_changeclock(void) {

	UINT32	clk;
	UINT	hz;
	UINT	hzmax;

	if (sndstream.buffer == NULL) {
		return;
	}

	// とりあえず 25で割り切れる。
	clk = pccore.realclock / 25;
	hz = soundcfg.rate / 25;

	// で、クロック数に合せて調整。(64bit演算しろよな的)
	hzmax = (1 << (32 - 8)) / (clk >> 8);
	while(hzmax < hz) {
		clk = (clk + 1) >> 1;
		hz = (hz + 1) >> 1;
	}
	TRACEOUT(("hzbase/clockbase = %d/%d", hz, clk));
	soundcfg.hzbase = hz;
	soundcfg.clockbase = clk;
	soundcfg.minclock = 2 * clk / hz;
	soundcfg.lastclock = CPU_CLOCK;
}

void sound_streamregist(void *hdl, SOUNDCB cbfn) {

	if (sndstream.buffer) {
		if ((cbfn) &&
			(sndstream.cbreg < (sndstream.cb + STREAM_CBMAX))) {
			sndstream.cbreg->hdl = hdl;
			sndstream.cbreg->cbfn = cbfn;
			sndstream.cbreg++;
		}
	}
}


// ----

void sound_sync(void) {

	UINT32	length;

	if (sndstream.buffer == NULL) {
		return;
	}

	length = CPU_CLOCK + CPU_BASECLOCK - CPU_REMCLOCK - soundcfg.lastclock;
	if (length < soundcfg.minclock) {
		return;
	}
	length = (length * soundcfg.hzbase) / soundcfg.clockbase;
	if (length == 0) {
		return;
	}
	SNDCSEC_ENTER;
#if defined(SUPPORT_WAVEREC)
	if (sndstream.rec) {
		streamfilewrite(length);
	}
	else
#endif
		streamprepare(length);
	soundcfg.lastclock += length * soundcfg.clockbase / soundcfg.hzbase;
	beep_eventreset();
	SNDCSEC_LEAVE;

	soundcfg.writecount += length;
	if (soundcfg.writecount >= 100) {
		soundcfg.writecount = 0;
		soundmng_sync();
	}
}

static volatile int locks = 0;

const SINT32 *sound_pcmlock(void) {

const SINT32 *ret;

	if (locks) {
		TRACEOUT(("sound pcm lock: already locked"));
		return(NULL);
	}
	locks++;
	ret = sndstream.buffer;
	if (ret) {
		SNDCSEC_ENTER;
		if (sndstream.remain > sndstream.reserve)
#if defined(SUPPORT_WAVEREC)
			if (sndstream.rec) {
				filltailsample(sndstream.remain - sndstream.reserve);
			}
			else
#endif
		{
			streamprepare(sndstream.remain - sndstream.reserve);
			soundcfg.lastclock = CPU_CLOCK + CPU_BASECLOCK - CPU_REMCLOCK;
			beep_eventreset();
		}
	}
	else {
		locks--;
	}
	return(ret);
}

void sound_pcmunlock(const SINT32 *hdl) {

	int		leng;

	if (hdl) {
		leng = sndstream.reserve - sndstream.remain;
		if (leng > 0) {
			CopyMemory(sndstream.buffer,
						sndstream.buffer + (sndstream.samples * 2),
												leng * 2 * sizeof(SINT32));
		}
		sndstream.ptr = sndstream.buffer + (leng * 2);
		sndstream.remain = sndstream.samples + sndstream.reserve - leng;
//		sndstream.remain += sndstream.samples;
		SNDCSEC_LEAVE;
		locks--;
	}
}


// ---- pcmmix

BRESULT pcmmix_regist(PMIXDAT *dat, void *datptr, UINT datsize, UINT rate) {

	GETSND	gs;
	UINT8	tmp[256];
	UINT	size;
	UINT	r;
	SINT16	*buf;

	gs = getsnd_create(datptr, datsize);
	if (gs == NULL) {
		goto pmr_err1;
	}
	if (getsnd_setmixproc(gs, rate, 1) != SUCCESS) {
		goto pmr_err2;
	}
	size = 0;
	do {
		r = getsnd_getpcmbyleng(gs, tmp, sizeof(tmp));
		size += r;
	} while(r);
	getsnd_destroy(gs);
	if (size == 0) {
		goto pmr_err1;
	}

	buf = (SINT16 *)_MALLOC(size, "PCM DATA");
	if (buf == NULL) {
		goto pmr_err1;
	}
	gs = getsnd_create(datptr, datsize);
	if (gs == NULL) {
		goto pmr_err1;
	}
	if (getsnd_setmixproc(gs, rate, 1) != SUCCESS) {
		goto pmr_err2;
	}
	r = getsnd_getpcmbyleng(gs, buf, size);
	getsnd_destroy(gs);
	dat->sample = buf;
	dat->samples = r / 2;
	return(SUCCESS);

pmr_err2:
	getsnd_destroy(gs);

pmr_err1:
	return(FAILURE);
}

BRESULT pcmmix_regfile(PMIXDAT *dat, const OEMCHAR *fname, UINT rate) {

	FILEH	fh;
	UINT	size;
	UINT8	*ptr;
	BRESULT	r;

	r = FAILURE;
	fh = file_open_rb(fname);
	if (fh == FILEH_INVALID) {
		goto pmrf_err1;
	}
	size = file_getsize(fh);
	if (size == 0) {
		goto pmrf_err2;
	}
	ptr = (UINT8 *)_MALLOC(size, fname);
	if (ptr == NULL) {
		goto pmrf_err2;
	}
	file_read(fh, ptr, size);
	file_close(fh);
	r = pcmmix_regist(dat, ptr, size, rate);
	_MFREE(ptr);
	return(r);

pmrf_err2:
	file_close(fh);

pmrf_err1:
	return(FAILURE);
}

void SOUNDCALL pcmmix_getpcm(PCMMIX hdl, SINT32 *pcm, UINT count) {

	UINT32		bitmap;
	PMIXTRK		*t;
const SINT16	*s;
	UINT		srem;
	SINT32		*d;
	UINT		drem;
	UINT		r;
	UINT		j;
	UINT		flag;
	SINT32		vol;
	SINT32		samp;

	if ((hdl->hdr.playing == 0) || (count == 0))  {
		return;
	}
	t = hdl->trk;
	bitmap = 1;
	do {
		if (hdl->hdr.playing & bitmap) {
			s = t->pcm;
			srem = t->remain;
			d = pcm;
			drem = count;
			flag = t->flag;
			vol = t->volume;
			do {
				r = min(srem, drem);
				switch(flag & (PMIXFLAG_L | PMIXFLAG_R)) {
					case PMIXFLAG_L:
						for (j=0; j<r; j++) {
							d[j*2+0] += (s[j] * vol) >> 12;
						}
						break;

					case PMIXFLAG_R:
						for (j=0; j<r; j++) {
							d[j*2+1] += (s[j] * vol) >> 12;
						}
						break;

					case PMIXFLAG_L | PMIXFLAG_R:
						for (j=0; j<r; j++) {
							samp = (s[j] * vol) >> 12;
							d[j*2+0] += samp;
							d[j*2+1] += samp;
						}
						break;
				}
				s += r;
				d += r*2;
				srem -= r;
				if (srem == 0) {
					if (flag & PMIXFLAG_LOOP) {
						s = t->data.sample;
						srem = t->data.samples;
					}
					else {
						hdl->hdr.playing &= ~bitmap;
						break;
					}
				}
				drem -= r;
			} while(drem);
			t->pcm = s;
			t->remain = srem;
		}
		t++;
		bitmap <<= 1;
	} while(bitmap < hdl->hdr.enable);
}

