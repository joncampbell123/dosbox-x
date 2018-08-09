
#if !defined(SUPPORT_PC9861K)

#define pc9861k_initialize()
#define	pc9861k_deinitialize()
#define	pc9861k_midipanic()

#else

typedef struct {
	UINT8	result;
	UINT8	data;
	UINT8	signal;
	UINT8	send;

	UINT	pos;
	UINT	dummyinst;
	UINT32	speed;
	SINT32	clk;

	UINT8	dip;
	UINT8	vect;
	UINT8	irq;
} _PC9861CH, *PC9861CH;

typedef struct {
	_PC9861CH	ch1;
	_PC9861CH	ch2;
	BOOL		en;
} _PC9861K, *PC9861K;


#ifdef __cplusplus
extern "C" {
#endif

extern const UINT32 pc9861k_speed[11];
extern _PC9861K	pc9861k;

void pc9861ch1cb(NEVENTITEM item);
void pc9861ch2cb(NEVENTITEM item);

void pc9861k_initialize(void);
void pc9861k_deinitialize(void);

void pc9861k_reset(const NP2CFG *pConfig);
void pc9861k_bind(void);

void pc9861k_midipanic(void);

#ifdef __cplusplus
}
#endif

#endif

