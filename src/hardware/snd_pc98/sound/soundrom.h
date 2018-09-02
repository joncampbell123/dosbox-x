
typedef struct {
	OEMCHAR	name[24];
	UINT32	address;
} SOUNDROM;


#ifdef __cplusplus
extern "C" {
#endif

extern	SOUNDROM	soundrom;

void soundrom_reset(void);
void soundrom_load(UINT32 address, const OEMCHAR *primary);
void soundrom_loadex(UINT sw, const OEMCHAR *primary);

#ifdef __cplusplus
}
#endif

