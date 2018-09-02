
#if defined(SUPPORT_KEYDISP)

#include	"cmndraw.h"

struct _cmnpalfn {
	UINT8	(*get8)(struct _cmnpalfn *fn, UINT num);
	UINT32	(*get32)(struct _cmnpalfn *fn, UINT num);
	UINT16	(*cnv16)(struct _cmnpalfn *fn, RGB32 pal32);
	long	userdata;
};
typedef struct _cmnpalfn	CMNPALFN;

typedef struct {
	UINT8	pal8;
	UINT8	padding;
	UINT16	pal16;
	RGB32	pal32;
} CMNPALS;


enum {
	KEYDISP_MODENONE			= 0,
	KEYDISP_MODEFM,
	KEYDISP_MODEMIDI
};

#if defined(SUPPORT_PX)
enum {
	KEYDISP_CHMAX		= 39,
	KEYDISP_FMCHMAX		= 30,
	KEYDISP_PSGMAX		= 3
};
#else	// defined(SUPPORT_PX)
enum {
	KEYDISP_CHMAX		= 16,
	KEYDISP_FMCHMAX		= 12,
	KEYDISP_PSGMAX		= 3
};
#endif	// defined(SUPPORT_PX)

enum {
	KEYDISP_NOTEMAX		= 16,

	KEYDISP_KEYCX		= 28,
	KEYDISP_KEYCY		= 14,

	KEYDISP_LEVEL		= (1 << 4),

	KEYDISP_WIDTH		= 301,
	KEYDISP_HEIGHT		= (KEYDISP_KEYCY * KEYDISP_CHMAX) + 1,

	KEYDISP_DELAYEVENTS	= 2048,
};

enum {
	KEYDISP_PALBG		= 0,
	KEYDISP_PALFG,
	KEYDISP_PALHIT,

	KEYDISP_PALS
};

enum {
	KEYDISP_FLAGDRAW		= 0x01,
	KEYDISP_FLAGREDRAW		= 0x02,
	KEYDISP_FLAGSIZING		= 0x04
};


#ifdef __cplusplus
extern "C" {
#endif

void keydisp_initialize(void);
void keydisp_setmode(UINT8 mode);
void keydisp_setpal(CMNPALFN *palfn);
void keydisp_setdelay(UINT8 frames);
UINT8 keydisp_process(UINT8 framepast);
void keydisp_getsize(int *width, int *height);
BOOL keydisp_paint(CMNVRAM *vram, BOOL redraw);

void keydisp_setfmboard(UINT board);
void keydisp_fmkeyon(UINT8 ch, UINT8 value);
void keydisp_psgmix(void *psg);
void keydisp_psgvol(void *psg, UINT8 ch);
void keydisp_midi(const UINT8 *msg);

#ifdef __cplusplus
}
#endif

#else

#define keydisp_draw(a)
#define keydisp_setfmboard(a)
#define keydisp_fmkeyon(a, b)
#define keydisp_psgmix(a)
#define keydisp_psgvol(a, b)
#define	keydisp_midi(a)

#endif

