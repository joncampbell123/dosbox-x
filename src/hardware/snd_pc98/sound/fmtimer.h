
typedef struct {
	UINT16	timera;
	UINT8	timerb;
	UINT8	status;
	UINT8	reg;
	UINT8	intr;
	UINT8	irq;
	UINT8	intdisabel;
} _FMTIMER, *FMTIMER;


#ifdef __cplusplus
extern "C" {
#endif

void fmport_a(NEVENTITEM item);
void fmport_b(NEVENTITEM item);

void fmtimer_reset(UINT irq);
void fmtimer_setreg(UINT reg, REG8 value);

#ifdef __cplusplus
}
#endif

