#include	"compiler.h"
#include	"getsnd.h"


#if defined(SUPPORT_OGG)
#include	<math.h>
#include	"vorbis/codec.h"

typedef struct {
	int					phase;
	ogg_sync_state		oy;
	ogg_stream_state	os;
	ogg_page			og;
	ogg_packet			op;

	vorbis_info			vi;
	vorbis_comment		vc;
	vorbis_dsp_state	vd;
	vorbis_block		vb;
} __OV;


// ----

static int snd_read(GETSND snd, void *buffer, int size) {

	UINT	rsize;

	if (size <= 0) {
		goto sndrd_err;
	}
	rsize = (UINT)size;
	if (rsize >= snd->datsize) {
		rsize = snd->datsize;
	}
	CopyMemory(buffer, snd->datptr, rsize);
	snd->datptr += rsize;
	snd->datsize -= rsize;
	return((int)rsize);

sndrd_err:
	return(0);
}

enum {
	OVPHASE_HEAD		= 0,
	OVPHASE_STREAMIN,
	OVPHASE_GETPCM,
	OVPHASE_NEXT,
	OVPHASE_CLOSE
};

static UINT ogg_dec(GETSND snd, short *dst) {

	__OV	*ov;
	int		result;
	char	*buffer;
	int		bytes;
	float	**pcm;
	int		samples;
	int		i;
	int		j;
	float	*mono;
	short	*ptr;
	long	val;

	ov = (__OV *)snd->snd;

	do {
		switch(ov->phase) {
			case OVPHASE_HEAD:
				result = ogg_sync_pageout(&ov->oy, &ov->og);
				if (result > 0) {
					ogg_stream_pagein(&ov->os, &ov->og);
					ov->phase = OVPHASE_STREAMIN;
				}
				else if (result == 0) {
					ov->phase = OVPHASE_NEXT;
				}
				else {
					TRACEOUT(("Corrupt or missing data in bitstream"));
				}
				break;

			case OVPHASE_STREAMIN:
				result = ogg_stream_packetout(&ov->os, &ov->op);
				if (result > 0) {
					if (vorbis_synthesis(&ov->vb, &ov->op) == 0) {
						vorbis_synthesis_blockin(&ov->vd, &ov->vb);
					}
					ov->phase = OVPHASE_GETPCM;
				}
				else if (result == 0) {
					if (!ogg_page_eos(&ov->og)) {
						ov->phase = OVPHASE_NEXT;
					}
					else {
						ov->phase = OVPHASE_CLOSE;
					}
				}
				break;

			case OVPHASE_GETPCM:
				samples = vorbis_synthesis_pcmout(&ov->vd, &pcm);
				if (samples > 0) {
					if (samples > (int)snd->blocksamples) {
						samples = (int)snd->blocksamples;
					}
					for (i=0; i<ov->vi.channels; i++) {
						ptr = dst + i;
						mono = pcm[i];
						for (j=0; j<samples; j++) {
							val = (long)(mono[j] * 32767.f);
							if (val > 32767) {
								val = 32767;
							}
							if (val < -32768) {
								val = -32768;
							}
							*ptr = (short)val;
							ptr += ov->vi.channels;
						}
					}
					vorbis_synthesis_read(&ov->vd, samples);
					return((UINT)samples);
				}
				ov->phase = OVPHASE_STREAMIN;
				break;

			case OVPHASE_NEXT:
				buffer = ogg_sync_buffer(&ov->oy, 4096);
				bytes = snd_read(snd, buffer, 4096);
				ogg_sync_wrote(&ov->oy, bytes);
#if 1
				ov->phase = OVPHASE_HEAD;
#else
				if (bytes) {
					ov->phase = OVPHASE_HEAD;
				}
				else {
					ov->phase = OVPHASE_CLOSE;
				}
#endif
				break;

			case OVPHASE_CLOSE:
				return(0);
		}
	} while(1);
}

static void ogg_decend(GETSND snd) {

	__OV	*ov;

	ov = (__OV *)snd->snd;

	ogg_stream_clear(&ov->os);
	vorbis_block_clear(&ov->vb);
	vorbis_dsp_clear(&ov->vd);
	vorbis_comment_clear(&ov->vc);
	vorbis_info_clear(&ov->vi);
	ogg_sync_clear(&ov->oy);
	_MFREE(ov);
}

BOOL getogg_open(GETSND snd, UINT8 *ptr, UINT size) {

	__OV	*ov;
	char	*buffer;
	int		bytes;
	int 	i;
	int		result;

	snd->datptr = ptr;
	snd->datsize = size;

	ov = (__OV *)_MALLOC(sizeof(__OV), "__OV");
	if (ov == NULL) {
		goto ovopn_err0;
	}
	ZeroMemory(ov, sizeof(__OV));

	buffer = ogg_sync_buffer(&ov->oy, 4096);
	bytes = snd_read(snd, buffer, 4096);
	ogg_sync_wrote(&ov->oy, bytes);

	if (ogg_sync_pageout(&ov->oy, &ov->og) != 1) {
		TRACEOUT(("Input does not appear to be an Ogg bitstream."));
		goto ovopn_err1;
	}
	ogg_stream_init(&ov->os, ogg_page_serialno(&ov->og));

	vorbis_info_init(&ov->vi);
	vorbis_comment_init(&ov->vc);
	if (ogg_stream_pagein(&ov->os, &ov->og) < 0) {
		TRACEOUT(("Error reading first page of Ogg bitstream data."));
		goto ovopn_err1;
	}

	if (ogg_stream_packetout(&ov->os, &ov->op) != 1) {
		TRACEOUT(("Error reading initial header packet."));
		goto ovopn_err1;
	}

	if (vorbis_synthesis_headerin(&ov->vi, &ov->vc, &ov->op) < 0) {
		TRACEOUT(("This Ogg bitstream does not contain Vorbis audio data."));
		goto ovopn_err1;
	}

	i = 0;
	while(i < 2) {
		while(i < 2) {
			result = ogg_sync_pageout(&ov->oy, &ov->og);
			if (result == 0) {
				break;
			}
			if (result == 1) {
				ogg_stream_pagein(&ov->os, &ov->og);
				while(i < 2) {
					result = ogg_stream_packetout(&ov->os, &ov->op);
					if (result == 0) {
						break;
					}
					if (result < 0) {
						TRACEOUT(("Corrupt secondary header. Exiting."));
						goto ovopn_err1;
					}
					vorbis_synthesis_headerin(&ov->vi, &ov->vc, &ov->op);
					i++;
				}
			}
		}
		buffer = ogg_sync_buffer(&ov->oy, 4096);
		bytes = snd_read(snd, buffer, 4096);
		if ((bytes == 0) && (i < 2)) {
			TRACEOUT(("End of file before finding all Vorbis headers!"));
			return(-1);
		}
		ogg_sync_wrote(&ov->oy, bytes);
	}

	snd->snd = ov;
	snd->dec = (GSDEC)ogg_dec;
	snd->decend = ogg_decend;
	snd->samplingrate = ov->vi.rate;
	snd->channels = ov->vi.channels;
	snd->blocksize = 4096 * 2;
	snd->blocksamples = 4096 / ov->vi.channels;
	snd->bit = 16;

	vorbis_synthesis_init(&ov->vd, &ov->vi);
	vorbis_block_init(&ov->vd, &ov->vb);
	return(SUCCESS);

ovopn_err1:
	ogg_sync_clear(&ov->oy);
	_MFREE(ov);

ovopn_err0:
	return(FAILURE);
}
#endif

