/* The purpose of this header is to provide defines and macros
 * to help port code from Neko Project II and match the typedefs
 * it uses. */

#include <math.h>
#include <stdio.h>
#include <dirent.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#if !defined(MAX_PATH)
#define MAX_PATH PATH_MAX
#endif

#ifdef WIN32
# define BYTESEX_LITTLE
# define _G_DIR_SEPARATOR '\\'
#else
# define _G_DIR_SEPARATOR '/'
# include <endian.h>
# if BYTE_ORDER == LITTLE_ENDIAN
# define BYTESEX_LITTLE
# else
# define BYTESEX_BIG
# endif
#endif

enum {
	SUCCESS		= 0,
	FAILURE		= 1
};

enum {
	PCBASECLOCK25		= 2457600,
	PCBASECLOCK20		= 1996800
};

enum {
	CPUMODE_8MHZ		= 0x20,

	PCMODEL_VF			= 0,
	PCMODEL_VM			= 1,
	PCMODEL_VX			= 2,
	PCMODELMASK			= 0x3f,
	PCMODEL_PC9821		= 0x40,
	PCMODEL_EPSON		= 0x80,

	PCHDD_SASI			= 0x01,
	PCHDD_SCSI			= 0x02,
	PCHDD_IDE			= 0x04,

	PCROM_BIOS			= 0x01,
	PCROM_SOUND			= 0x02,
	PCROM_SASI			= 0x04,
	PCROM_SCSI			= 0x08,
	PCROM_BIOS9821		= 0x10,

	PCSOUND_NONE		= 0x00,

	PCCBUS_PC9861K		= 0x0001,
	PCCBUS_MPU98		= 0x0002
};

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
typedef uint8_t BYTE;
#endif
typedef char OEMCHAR;
typedef void* NEVENTITEM;
#define OEMTEXT(x) (x)
#define SOUNDCALL

static uint16_t LOADINTELWORD(void *x) {
    return *((uint16_t*)(x));
}

static void STOREINTELWORD(void *x,uint16_t y) {
    *((uint16_t*)(x)) = y;
}

static uint32_t LOADINTELDWORD(void *x) {
    return *((uint32_t*)(x));
}

static void STOREINTELDWORD(void *x,uint32_t y) {
    *((uint32_t*)(x)) = y;
}

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

#ifndef PI
#define PI M_PI
#endif

#ifndef WIN32
static inline void ZeroMemory(void *p,size_t l) {
    memset(p,0,l);
}

static inline void FillMemory(void *p,size_t l,unsigned char c) {
    memset(p,c,l);
}
#endif

static inline void pcm86io_bind(void) {
    /* dummy */
}

#ifdef __cplusplus
extern "C" {
#endif

void sound_sync(void);

#ifdef __cplusplus
}
#endif

#define BRESULT             UINT8

#ifdef __cplusplus
extern "C" {
#endif

#if 1
void _TRACEOUT(const char *fmt,...);
#else
static inline void _TRACEOUT(const char *fmt,...) { };
#endif
#define TRACEOUT(a) _TRACEOUT a

#ifdef __cplusplus
}
#endif

typedef struct {
	// G~[gÉæ­QÆ³êéz
	UINT8	uPD72020;
	UINT8	DISPSYNC;
	UINT8	RASTER;
	UINT8	realpal;
	UINT8	LCD_MODE;
	UINT8	skipline;
	UINT16	skiplight;

	UINT8	KEY_MODE;
	UINT8	XSHIFT;
	UINT8	BTN_RAPID;
	UINT8	BTN_MODE;

	UINT8	dipsw[3];
	UINT8	MOUSERAPID;

	UINT8	calendar;
	UINT8	usefd144;
	UINT8	wait[6];

	// ZbgÆ© ñÜèQÆ³êÈ¢z
	OEMCHAR	model[8];
	UINT	baseclock;
	UINT	multiple;

	UINT8	memsw[8];

	UINT8	ITF_WORK;
	UINT8	EXTMEM;
	UINT8	grcg;
	UINT8	color16;
	UINT32	BG_COLOR;
	UINT32	FG_COLOR;

	UINT16	samplingrate;
	UINT16	delayms;
	UINT8	SOUND_SW;
	UINT8	snd_x;

	UINT8	snd14opt[3];
	UINT8	snd26opt;
	UINT8	snd86opt;
	UINT8	spbopt;
	UINT8	spb_vrc;												// ver0.30
	UINT8	spb_vrl;												// ver0.30
	UINT8	spb_x;													// ver0.30

	UINT8	BEEP_VOL;
	UINT8	vol14[6];
	UINT8	vol_fm;
	UINT8	vol_ssg;
	UINT8	vol_adpcm;
	UINT8	vol_pcm;
	UINT8	vol_rhythm;

	UINT8	mpuenable;
	UINT8	mpuopt;

	UINT8	pc9861enable;
	UINT8	pc9861sw[3];
	UINT8	pc9861jmp[6];

	UINT8	fddequip;
	UINT8	MOTOR;
	UINT8	MOTORVOL;
	UINT8	PROTECTMEM;
	UINT8	hdrvacc;

	OEMCHAR	sasihdd[2][MAX_PATH];									// ver0.74
#if defined(SUPPORT_SCSI)
	OEMCHAR	scsihdd[4][MAX_PATH];									// ver0.74
#endif
	OEMCHAR	fontfile[MAX_PATH];
	OEMCHAR	biospath[MAX_PATH];
	OEMCHAR	hdrvroot[MAX_PATH];
} NP2CFG;

#ifdef __cplusplus
extern "C" {
#endif

unsigned char *CGetMemBase();

void getbiospath(OEMCHAR *path, const OEMCHAR *fname, int maxlen);

REG8 joymng_getstat(void);
REG8 keystat_getjoy(void);

extern NP2CFG pccore;
#define np2cfg pccore

#ifdef __cplusplus
}
#endif

#ifdef reg
#undef reg
#endif

#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif

#ifndef	NELEMENTS
#define	NELEMENTS(a)	((int)(sizeof(a) / sizeof(a[0])))
#endif

#define	_MEM_INIT()				
#define	_MALLOC(a, b)			malloc(a)
#define	_MFREE(a)				free(a)
#define	_HANDLE_ADD(a, b)		
#define	_HANDLE_REM(a)			
#define	_MEM_USED(a)			

#define CopyMemory(d,s,n)   memcpy((d), (s), (n))

typedef FILE *			FILEH;

#ifndef	FASTCALL
#define	FASTCALL
#endif

#define	IOOUTCALL	FASTCALL
#define	IOINPCALL	FASTCALL

typedef	void (IOOUTCALL *IOOUT)(UINT port, REG8 val);
typedef	REG8 (IOINPCALL *IOINP)(UINT port);

#ifdef __cplusplus
extern "C" {
#endif

void cbuscore_attachsndex(UINT port, const IOOUT *out, const IOINP *inp);

#ifdef __cplusplus
}
#endif

