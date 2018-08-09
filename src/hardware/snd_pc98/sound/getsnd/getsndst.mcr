
#if (MIX_INPUTBIT == 8)
#define	_SMP_IN		unsigned char
#define	_SAMP(s)	(((long)(s) - 0x80) << 8)
#elif (MIX_INPUTBIT == 16)
#define	_SMP_IN		short
#define	_SAMP(s)	(long)(s)
#endif


#if (MIX_OUTPUTBIT == 8)
#define	_SMP_OUT		unsigned char
#define	_SAMPLIMIT(s)	if ((s) > 32767) {				\
							(s) = 32767;				\
						}								\
						else if ((s) < -32768) {		\
							(s) = -32768;				\
						}
#define	_OUTSAMP(d, s)	(d) = (_SMP_OUT)(((s) >> 8) + 0x80)
#elif (MIX_OUTPUTBIT == 16)
#define	_SMP_OUT		short
#define	_SAMPLIMIT(s)	if ((s) > 32767) {				\
							(s) = 32767;				\
						}								\
						else if ((s) < -32768) {		\
							(s) = -32768;				\
						}
#define	_OUTSAMP(d, s)	(d) = (_SMP_OUT)(s)
#endif


static _SMP_OUT *FUNC_NOR(GETSND trk, _SMP_OUT *pcm, _SMP_OUT *pcmterm) {

	_SMP_IN	*samp;
	UINT	size;

	size = min(trk->remain, (UINT)((pcmterm - pcm) / 2));
	trk->remain -= size;
	samp = (_SMP_IN *)trk->buf;
	do {
		long out;
		out = _SAMP(*samp++);
		_SAMPLIMIT(out);
		_OUTSAMP(pcm[0], out);
#if (MIX_CHANNELS == 2)
		out = _SAMP(*samp++);
		_SAMPLIMIT(out);
#endif
		_OUTSAMP(pcm[1], out);
		pcm += 2;
	} while(--size);
	trk->buf = samp;
	return(pcm);
}

static _SMP_OUT *FUNC_DOWN(GETSND trk, _SMP_OUT *pcm, _SMP_OUT *pcmterm) {

	long	mrate;
	_SMP_IN	*samp;

	samp = (_SMP_IN *)trk->buf;
	mrate = trk->mrate;
	do {
		if (trk->rem > mrate) {
			trk->rem -= mrate;
			trk->pcml += _SAMP(*samp++) * mrate;
#if (MIX_CHANNELS == 2)
			trk->pcmr += _SAMP(*samp++) * mrate;
#endif
		}
		else {
			long out;
			long tmp;
			out = trk->pcml;
			out += _SAMP(samp[0]) * trk->rem;
			out >>= DNBASEBITS;
			_SAMPLIMIT(out);
			_OUTSAMP(pcm[0], out);
#if (MIX_CHANNELS == 2)
			out = trk->pcmr;
			out += _SAMP(samp[1]) * trk->rem;
			out >>= DNBASEBITS;
			_SAMPLIMIT(out);
#endif
			_OUTSAMP(pcm[1], out);
			tmp = mrate - trk->rem;
			trk->pcml = _SAMP(*samp++) * tmp;
#if (MIX_CHANNELS == 2)
			trk->pcmr = _SAMP(*samp++) * tmp;
#endif
			trk->rem = DNMIXBASE - tmp;
			pcm += 2;
			if (pcm >= pcmterm) {
				trk->remain--;
				break;
			}
		}
	} while(--trk->remain);
	trk->buf = samp;
	return(pcm);
}

static _SMP_OUT *FUNC_UP(GETSND trk, _SMP_OUT *pcm, _SMP_OUT *pcmterm) {

	_SMP_IN	*samp;
	long	mrate;

	samp = (_SMP_IN *)trk->buf;
	mrate = trk->mrate;
	do {
		long tmp;
		tmp = UPMIXBASE - trk->rem;
		if (tmp >= 0) {
			long dat;
			long next;
			dat = (trk->pcml * trk->rem);
			next = _SAMP(*samp++);
			dat += (next * tmp);
			dat >>= UPBASEBITS;
			_SAMPLIMIT(dat);
			trk->pcml = next;
			_OUTSAMP(pcm[0], dat);
#if (MIX_CHANNELS == 2)
			dat = (trk->pcmr * trk->rem);
			next = _SAMP(*samp++);
			dat += (next * tmp);
			dat >>= UPBASEBITS;
			_SAMPLIMIT(dat);
			trk->pcmr = next;
#endif
			_OUTSAMP(pcm[1], dat);
			trk->remain--;
			trk->rem = trk->mrate - tmp;
			pcm += 2;
			if (pcm >= pcmterm) {
				goto upsampterm;
			}
		}
		while(trk->rem >= UPMIXBASE) {
			long out;
			trk->rem -= UPMIXBASE;
			out = trk->pcml;
			_SAMPLIMIT(out);
			_OUTSAMP(pcm[0], out);
#if (MIX_CHANNELS == 2)
			out = trk->pcmr;
			_SAMPLIMIT(out);
#endif
			_OUTSAMP(pcm[1], out);
			pcm += 2;
			if (pcm >= pcmterm) {
				goto upsampterm;
			}
		}
	} while(trk->remain);
upsampterm:
	trk->buf = samp;
	return(pcm);
}

#undef _SAMP
#undef _SMP_IN
#undef _SMP_OUT
#undef _SAMPLIMIT
#undef _OUTSAMP

#undef MIX_INPUTBIT
#undef MIX_OUTPUTBIT
#undef MIX_CHANNELS
#undef FUNC_NOR
#undef FUNC_DOWN
#undef FUNC_UP

