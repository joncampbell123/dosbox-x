/* The purpose of this header is to provide defines and macros
 * to help port code from Neko Project II and match the typedefs
 * it uses. */

#include <math.h>

#include "inout.h"
#include "mixer.h"

// GLUE TYPEDEFS
// WARNING: Windows targets will want to IFDEF some of these out as they will
//          conflict with the typedefs in windows.h
typedef uint32_t UINT32;
typedef int32_t SINT32;
typedef uint16_t UINT16;
typedef int16_t SINT16;
typedef uint8_t UINT8;
typedef int8_t SINT8;
typedef uint32_t UINT;
typedef uint32_t REG8; /* GLIB guint32 -> UINT -> REG8 */
#ifndef WIN32
typedef uint8_t BOOL;
#endif
typedef char OEMCHAR;
typedef void* NEVENTITEM;
#define OEMTEXT(x) (x)
#define SOUNDCALL

#define LOADINTELWORD(x) host_readw((HostPt)(x))
#define STOREINTELWORD(x,y) host_writew((HostPt)(x),(y))

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* TODO: Move into another header */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum {
	PCBASECLOCK25		= 2457600,
	PCBASECLOCK20		= 1996800
};

#ifndef WIN32
static inline void ZeroMemory(void *p,size_t l) {
    memset(p,0,l);
}

static inline void FillMemory(void *p,size_t l,unsigned char c) {
    memset(p,c,l);
}
#endif

extern MixerChannel *pc98_mixer;

static inline void pcm86io_bind(void) {
    /* dummy */
}

static inline void sound_sync(void) {
    if (pc98_mixer) pc98_mixer->FillUp();
}

#ifndef IOOUTCALL
#define	IOOUTCALL
#endif

#ifndef IOINPCALL
#define	IOINPCALL
#endif

#ifndef BRESULT
#define	BRESULT		UINT
#endif

enum {
	SUCCESS		= 0,
	FAILURE		= 1
};

// HACK: Map the Neko Project II I/O handlers to DOSBox-X I/O handlers
//typedef Bitu IO_ReadHandler(Bitu port,Bitu iolen);
//typedef void IO_WriteHandler(Bitu port,Bitu val,Bitu iolen);
typedef IO_ReadHandler *IOINP;
typedef IO_WriteHandler *IOOUT;

//typedef void (IOOUTCALL *IOOUT)(UINT port, REG8 val);
//typedef REG8 (IOINPCALL *IOINP)(UINT port);

void cbuscore_attachsndex(UINT port, const IOOUT *out, const IOINP *inp);

// TEhI/O - 12bit decode
BRESULT iocore_attachsndout(UINT port, IOOUT func);
BRESULT iocore_attachsndinp(UINT port, IOINP func);

typedef struct {
	UINT	baseclock;
	UINT	multiple;

    UINT16	samplingrate;

	UINT8	snd86opt;

	UINT8	vol14[6];
	UINT8	vol_fm;
	UINT8	vol_ssg;
	UINT8	vol_adpcm;
	UINT8	vol_pcm;
	UINT8	vol_rhythm;
} NP2CFG;

extern NP2CFG      np2_cfg;

