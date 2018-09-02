
#ifndef SOUNDCALL
#define	SOUNDCALL
#endif

#if !defined(DISABLE_SOUND)

typedef void (SOUNDCALL * SOUNDCB)(void *hdl, SINT32 *pcm, UINT count);

typedef struct {
	UINT	rate;
	UINT32	hzbase;
	UINT32	clockbase;
	UINT32	minclock;
	UINT32	lastclock;
	UINT	writecount;
} SOUNDCFG;


#ifdef __cplusplus
extern "C" {
#endif

extern	SOUNDCFG	soundcfg;

BOOL sound_create(UINT rate, UINT ms);
void sound_destroy(void);

void sound_reset(void);
void sound_changeclock(void);
void sound_streamregist(void *hdl, SOUNDCB cbfn);

void sound_sync(void);

const SINT32 *sound_pcmlock(void);
void sound_pcmunlock(const SINT32 *hdl);

#if defined(SUPPORT_WAVEREC)
BOOL sound_recstart(const OEMCHAR *filename);
void sound_recstop(void);
#endif

#ifdef __cplusplus
}
#endif



// ---- PCM MIX

enum {
	PMIXFLAG_L		= 0x0001,
	PMIXFLAG_R		= 0x0002,
	PMIXFLAG_LOOP	= 0x0004
};

typedef struct {
	UINT32	playing;
	UINT32	enable;
} PMIXHDR;

typedef struct {
	SINT16	*sample;
	UINT	samples;
} PMIXDAT;

typedef struct {
const SINT16	*pcm;
	UINT		remain;
	PMIXDAT		data;
	UINT		flag;
	SINT32		volume;
} PMIXTRK;

typedef struct {
	PMIXHDR		hdr;
	PMIXTRK		trk[1];
} _PCMMIX, *PCMMIX;


#ifdef __cplusplus
extern "C" {
#endif

BRESULT pcmmix_regist(PMIXDAT *dat, void *datptr, UINT datsize, UINT rate);
BRESULT pcmmix_regfile(PMIXDAT *dat, const OEMCHAR *fname, UINT rate);

void SOUNDCALL pcmmix_getpcm(PCMMIX hdl, SINT32 *pcm, UINT count);

#ifdef __cplusplus
}
#endif

#else

#define sound_pcmlock()		(NULL)
#define sound_pcmunlock(h)
#define sound_reset()
#define sound_changeclock()
#define sound_sync()

#endif

