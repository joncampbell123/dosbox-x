
// #define	SUPPORT_MP3
// #define	SUPPORT_OGG


#ifdef __cplusplus
extern "C" {
#endif

struct _getsnd;
typedef	struct _getsnd		_GETSND;
typedef	struct _getsnd		*GETSND;

typedef UINT (*GSDEC)(GETSND self, void *buf);
typedef void (*GSDECEND)(GETSND self);
typedef void *(*GSCNV)(GETSND self, void *buf, void *bufterm);

BOOL getwave_open(GETSND snd, UINT8 *ptr, UINT size);
BOOL getmp3_open(GETSND snd, UINT8 *ptr, UINT size);
BOOL getogg_open(GETSND snd, UINT8 *ptr, UINT size);
BOOL getsnd_setmixproc(GETSND snd, UINT samprate, UINT channles);

struct _getsnd {
	UINT8		*work;					// data load用バッファ
	UINT8		*buffer;				// デコード済みバッファ

	void		*buf;
	UINT		remain;
	long		mrate;
	long		rem;
	long		pcml;
	long		pcmr;

	UINT8		*datptr;
	UINT		datsize;

	void		*snd;					// optional
	GSDEC		dec;
	GSDECEND	decend;					// optional
	GSCNV		cnv;

	UINT		samplingrate;
	UINT		channels;
	UINT		bit;
	UINT		blocksamples;			// ブロックサンプル数
	UINT		blocksize;				// １ブロックのワークサイズ
};

GETSND getsnd_create(void *datptr, UINT datsize);
void getsnd_destroy(GETSND hdl);

UINT getsnd_getpcmbyleng(GETSND hdl, void *pcm, UINT leng);

#ifdef __cplusplus
}
#endif

