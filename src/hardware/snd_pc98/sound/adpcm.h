
enum {
	ADTIMING_BIT	= 11,
	ADTIMING		= (1 << ADTIMING_BIT),
	ADPCM_SHIFT		= 3
};

typedef struct {
	UINT8	ctrl1;		// 00
	UINT8	ctrl2;		// 01
	UINT8	start[2];	// 02
	UINT8	stop[2];	// 04
	UINT8	reg06;
	UINT8	reg07;
	UINT8	data;		// 08
	UINT8	delta[2];	// 09
	UINT8	level;		// 0b
	UINT8	limit[2];	// 0c
	UINT8	reg0e;
	UINT8	reg0f;
	UINT8	flag;		// 10
	UINT8	reg11;
	UINT8	reg12;
	UINT8	reg13;
} ADPCMREG;

typedef struct {
	ADPCMREG	reg;
	UINT32		pos;
	UINT32		start;
	UINT32		stop;
	UINT32		limit;
	SINT32		level;
	UINT32		base;
	SINT32		samp;
	SINT32		delta;
	SINT32		remain;
	SINT32		step;
	SINT32		out0;
	SINT32		out1;
	SINT32		fb;
	SINT32		pertim;
	UINT8		status;
	UINT8		play;
	UINT8		mask;
	UINT8		fifopos;
	UINT8		fifo[2];
	UINT8		padding[2];
	UINT8		buf[0x40000];
} _ADPCM, *ADPCM;

typedef struct {
	UINT	rate;
	UINT	vol;
} ADPCMCFG;


#ifdef __cplusplus
extern "C" {
#endif

void adpcm_initialize(UINT rate);
void adpcm_setvol(UINT vol);

void adpcm_reset(ADPCM ad);
void adpcm_update(ADPCM ad);
void adpcm_setreg(ADPCM ad, UINT reg, REG8 value);
REG8 adpcm_status(ADPCM ad);

REG8 SOUNDCALL adpcm_readsample(ADPCM ad);
void SOUNDCALL adpcm_datawrite(ADPCM ad, REG8 data);
void SOUNDCALL adpcm_getpcm(ADPCM ad, SINT32 *buf, UINT count);

#ifdef __cplusplus
}
#endif

