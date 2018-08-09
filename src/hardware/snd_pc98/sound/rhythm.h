
typedef struct {
	PMIXHDR	hdr;
	PMIXTRK	trk[6];
	UINT	vol;
	UINT8	trkvol[8];
} _RHYTHM, *RHYTHM;


#ifdef __cplusplus
extern "C" {
#endif

void rhythm_initialize(UINT rate);
void rhythm_deinitialize(void);
UINT rhythm_getcaps(void);
void rhythm_setvol(UINT vol);

void rhythm_reset(RHYTHM rhy);
void rhythm_bind(RHYTHM rhy);
void rhythm_update(RHYTHM rhy);
void rhythm_setreg(RHYTHM rhy, UINT reg, REG8 val);

#ifdef __cplusplus
}
#endif

