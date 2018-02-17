
#if !defined(DISABLE_SOUND)

#include	"soundrom.h"
#include	"tms3631.h"
#include	"fmtimer.h"
#include	"opngen.h"
#include	"psggen.h"
#include	"rhythm.h"
#include	"adpcm.h"
#include	"pcm86.h"
#include	"cs4231.h"


typedef struct {
	UINT	addr;
	UINT	addr2;
	UINT8	data;
	UINT8	data2;
	UINT16	base;
	UINT8	adpcmmask;
	UINT8	channels;
	UINT8	extend;
	UINT8	_padding;
	UINT8	reg[0x400];
} OPN_T;

typedef struct {
	UINT16	port;
	UINT8	psg3reg;
	UINT8	rhythm;
} AMD98;

typedef struct {
	UINT8	porta;
	UINT8	portb;
	UINT8	portc;
	UINT8	mask;
	UINT8	key[8];
	int		sync;
	int		ch;
} MUSICGEN;


#ifdef __cplusplus
extern "C" {
#endif

extern	UINT32		usesound;
extern	OPN_T		opn;
extern	AMD98		amd98;
extern	MUSICGEN	musicgen;

extern	_TMS3631	tms3631;
extern	_FMTIMER	fmtimer;
extern	_OPNGEN		opngen;
extern	OPNCH		opnch[OPNCH_MAX];
extern	_PSGGEN		__psg[3];
extern	_RHYTHM		rhythm;
extern	_ADPCM		adpcm;
extern	_PCM86		pcm86;
extern	_CS4231		cs4231;

#define	psg1	__psg[0]
#define	psg2	__psg[1]
#define	psg3	__psg[2]

#if defined(SUPPORT_PX)
extern	OPN_T		opn2;
extern	OPN_T		opn3;
extern	_RHYTHM		rhythm2;
extern	_RHYTHM		rhythm3;
extern	_ADPCM		adpcm2;
extern	_ADPCM		adpcm3;
#endif	// defined(SUPPORT_PX)

REG8 fmboard_getjoy(PSGGEN psg);

void fmboard_extreg(void (*ext)(REG8 enable));
void fmboard_extenable(REG8 enable);

void fmboard_reset(const NP2CFG *pConfig, UINT32 type);
void fmboard_bind(void);

void fmboard_fmrestore(REG8 chbase, UINT bank);
void fmboard_rhyrestore(RHYTHM rhy, UINT bank);

#if defined(SUPPORT_PX)
void fmboard_fmrestore2(OPN_T* pOpn, REG8 chbase, UINT bank);
void fmboard_rhyrestore2(OPN_T* pOpn, RHYTHM rhy, UINT bank);
#endif	// defined(SUPPORT_PX)

#ifdef __cplusplus
}
#endif

#else

#define	fmboard_reset(t)
#define	fmboard_bind()

#endif

