
#ifdef __cplusplus
extern "C" {
#endif

void cbuscore_reset(const NP2CFG *pConfig);
void cbuscore_bind(void);

void cbuscore_attachsndex(UINT port, const IOOUT *out, const IOINP *inp);

#ifdef __cplusplus
}
#endif

