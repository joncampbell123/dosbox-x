
typedef struct {
	UINT32	freq;
	UINT32	count;
} TMSCH;

typedef struct {
	TMSCH	ch[8];
	UINT	enable;
} _TMS3631, *TMS3631;

typedef struct {
	UINT	ratesft;
	SINT32	left;
	SINT32	right;
	SINT32	feet[16];
} TMS3631CFG;


#ifdef __cplusplus
extern "C" {
#endif

void tms3631_initialize(UINT rate);
void tms3631_setvol(const UINT8 *vol);

void tms3631_reset(TMS3631 tms);
void tms3631_setkey(TMS3631 tms, REG8 ch, REG8 key);
void tms3631_setenable(TMS3631 tms, REG8 enable);

void SOUNDCALL tms3631_getpcm(TMS3631 tms, SINT32 *pcm, UINT count);

#ifdef __cplusplus
}
#endif

