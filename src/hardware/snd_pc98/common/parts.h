
#ifndef PARTSCALL
#define	PARTSCALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

void PARTSCALL rand_setseed(SINT32 seed);
SINT32 PARTSCALL rand_get(void);
UINT8 PARTSCALL AdjustAfterMultiply(UINT8 value);
UINT8 PARTSCALL AdjustBeforeDivision(UINT8 value);
UINT PARTSCALL sjis2jis(UINT sjis);
UINT PARTSCALL jis2sjis(UINT jis);
void PARTSCALL satuation_s16(SINT16 *dst, const SINT32 *src, UINT size);
void PARTSCALL satuation_s16x(SINT16 *dst, const SINT32 *src, UINT size);

#ifdef __cplusplus
}
#endif

