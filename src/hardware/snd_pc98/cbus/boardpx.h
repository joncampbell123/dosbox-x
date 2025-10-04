
#if defined(SUPPORT_PX)

#ifdef __cplusplus
extern "C" {
#endif

void boardpx1_reset(const NP2CFG *pConfig);
void boardpx1_bind(void);

void boardpx2_reset(const NP2CFG *pConfig);
void boardpx2_bind(void);

#ifdef __cplusplus
}
#endif

#endif	// defined(SUPPORT_PX)

