
// Ç†Ç∆Ç≈ GETSNDÇ…èÊÇËä∑Ç¶ÇÈÇ©Ç‡Åc

#if defined(BYTESEX_BIG)
#define	WAVE_SIG(a, b, c, d)	\
				(UINT32)((d) + ((c) << 8) + ((b) << 16) + ((a) << 24))
#else
#define	WAVE_SIG(a, b, c, d)	\
				(UINT32)((a) + ((b) << 8) + ((c) << 16) + ((d) << 24))
#endif

typedef struct {
	UINT32	sig;
	UINT8	size[4];
	UINT32	fmt;
} RIFF_HEADER;

typedef struct {
	UINT	sig;
	UINT8	size[4];
} WAVE_HEADER;

typedef struct {
	UINT8	format[2];
	UINT8	channel[2];
	UINT8	rate[4];
	UINT8	rps[4];
	UINT8	block[2];
	UINT8	bit[2];
} WAVE_INFOS;


// ---- write

typedef struct {
	long	fh;
	UINT	rate;
	UINT	bits;
	UINT	ch;
	UINT	size;

	UINT8	*ptr;
	UINT	remain;
	UINT8	buf[4096];
} _WAVEWR, *WAVEWR;


#ifdef __cplusplus
extern "C" {
#endif

WAVEWR wavewr_open(const OEMCHAR *filename, UINT rate, UINT bits, UINT ch);
UINT wavewr_write(WAVEWR hdl, const void *buf, UINT size);
void wavewr_close(WAVEWR hdl);

#ifdef __cplusplus
}
#endif

