
enum {
	PSGFREQPADBIT		= 12,
	PSGADDEDBIT			= 3
};

enum {
	PSGENV_INC			= 15,
	PSGENV_ONESHOT		= 16,
	PSGENV_LASTON		= 32,
	PSGENV_ONECYCLE		= 64
};

typedef struct {
	SINT32	freq;
	SINT32	count;
	SINT32	*pvol;			// !!
	UINT16	puchi;
	UINT8	pan;
	UINT8	padding;
} PSGTONE;

typedef struct {
	SINT32	freq;
	SINT32	count;
	UINT	base;
} PSGNOISE;

typedef struct {
	UINT8	tune[3][2];		// 0
	UINT8	noise;			// 6
	UINT8	mixer;			// 7
	UINT8	vol[3];			// 8
	UINT8	envtime[2];		// b
	UINT8	env;			// d
	UINT8	io1;
	UINT8	io2;
} PSGREG;

typedef struct {
	PSGTONE		tone[3];
	PSGNOISE	noise;
	PSGREG		reg;
	UINT16		envcnt;
	UINT16		envmax;
	UINT8		mixer;
	UINT8		envmode;
	UINT8		envvol;
	SINT8		envvolcnt;
	SINT32		evol;				// !!
	UINT		puchicount;
} _PSGGEN, *PSGGEN;

typedef struct {
	SINT32	volume[16];
	SINT32	voltbl[16];
	UINT	rate;
	UINT32	base;
	UINT16	puchidec;
} PSGGENCFG;


#ifdef __cplusplus
extern "C" {
#endif

void psggen_initialize(UINT rate);
void psggen_setvol(UINT vol);

void psggen_reset(PSGGEN psg);
void psggen_restore(PSGGEN psg);
void psggen_setreg(PSGGEN psg, UINT reg, REG8 val);
REG8 psggen_getreg(PSGGEN psg, UINT reg);
void psggen_setpan(PSGGEN psg, UINT ch, REG8 pan);

void SOUNDCALL psggen_getpcm(PSGGEN psg, SINT32 *pcm, UINT count);

#ifdef __cplusplus
}
#endif

