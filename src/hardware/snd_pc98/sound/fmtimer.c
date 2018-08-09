#include    "np2glue.h"
//#include	"compiler.h"
//#include	"cpucore.h"
//#include	"pccore.h"
//#include	"iocore.h"
#include	"sound.h"
#include	"fmboard.h"


/* DOSBox-X replacement for nevents here */
static unsigned char nevent_is_set[2] = {0,0};

enum {
    NEVENT_FMTIMERA=0,
    NEVENT_FMTIMERB
};

/* meaningless now */
#define NEVENT_ABSOLUTE 0

void pc98_fm_dosbox_fmtimer_setevent(unsigned int n,double dt);
void pc98_fm_dosbox_fmtimer_clearevent(unsigned int n);

static void nevent_set(unsigned int n,double dt) {
    pc98_fm_dosbox_fmtimer_setevent(n,dt);
    nevent_is_set[n] = 1;
}

static void nevent_reset(unsigned int n) {
    pc98_fm_dosbox_fmtimer_clearevent(n);
    nevent_is_set[n] = 0;
}

static void nevent_ack_event(unsigned int n) {
    nevent_is_set[n] = 0;
}

static unsigned int nevent_iswork(unsigned int n) {
    return nevent_is_set[n];
}


static const UINT8 irqtable[4] = {0x03, 0x0d, 0x0a, 0x0c};

/* added for DOSBox-X */
UINT8 fmtimer_index2irq(const UINT8 index) {
    return irqtable[index & 3];
}

UINT8 fmtimer_irq2index(const UINT8 irq) {
    unsigned int i;

    for (i=0;i < 4;i++) {
        if (irqtable[i] == irq)
            return i;
    }

    return 0x00;
}
/* end */

static void set_fmtimeraevent(BOOL absolute) {

	SINT32	l;

    nevent_ack_event(NEVENT_FMTIMERA);

	l = 18 * (1024 - fmtimer.timera);
#if 0
	if (pccore.cpumode & CPUMODE_8MHZ) {		// 4MHz
		l = (l * 1248 / 625) * pccore.multiple;
	}
	else {										// 5MHz
		l = (l * 1536 / 625) * pccore.multiple;
	}
//	TRACEOUT(("FMTIMER-A: %08x-%d", l, absolute));
	nevent_set(NEVENT_FMTIMERA, l, fmport_a, absolute);
#else
    /* if you do the math here, it becomes:
     * l_us = l * core_clock_speed * multiple
     * which can be expressed in seconds like this:
     * dt = l / 1000000 */
    double dt_ms = (double)l / 1000;
    nevent_set(NEVENT_FMTIMERA,dt_ms);
#endif
}

static void set_fmtimerbevent(BOOL absolute) {

	SINT32	l;

    nevent_ack_event(NEVENT_FMTIMERB);

	l = 288 * (256 - fmtimer.timerb);
#if 0
	if (pccore.cpumode & CPUMODE_8MHZ) {		// 4MHz
		l = (l * 1248 / 625) * pccore.multiple;
	}
	else {										// 5MHz
		l = (l * 1536 / 625) * pccore.multiple;
	}
//	TRACEOUT(("FMTIMER-B: %08x-%d", l, absolute));
	nevent_set(NEVENT_FMTIMERB, l, fmport_b, absolute);
#else
    double dt_ms = (double)l / 1000;
    nevent_set(NEVENT_FMTIMERB,dt_ms);
#endif
}


void fmport_a(NEVENTITEM item) {

	BOOL	intreq = FALSE;

//	if (item->flag & NEVENT_SETEVENT) {
		intreq = pcm86gen_intrq();
		if (fmtimer.reg & 0x04) {
			fmtimer.status |= 0x01;
			intreq = TRUE;
		}
		if (intreq) {
			pic_setirq(fmtimer.irq);
//			TRACEOUT(("fm int-A"));
		}

		set_fmtimeraevent(FALSE);
//	}
}

void fmport_b(NEVENTITEM item) {

	BOOL	intreq = FALSE;

//	if (item->flag & NEVENT_SETEVENT) {
		intreq = pcm86gen_intrq();
		if (fmtimer.reg & 0x08) {
			fmtimer.status |= 0x02;
			intreq = TRUE;
		}
		if (intreq) {
			pic_setirq(fmtimer.irq);
//			TRACEOUT(("fm int-B"));
		}

		set_fmtimerbevent(FALSE);
//	}
}

void fmtimer_reset(UINT irq) {

	ZeroMemory(&fmtimer, sizeof(fmtimer));
	fmtimer.intr = irq & 0xc0;
	fmtimer.intdisabel = irq & 0x10;
	fmtimer.irq = irqtable[irq >> 6];
//	pic_registext(fmtimer.irq);
}

void fmtimer_setreg(UINT reg, REG8 value) {

//	TRACEOUT(("fm %x %x [%.4x:%.4x]", reg, value, CPU_CS, CPU_IP));

	switch(reg) {
		case 0x24:
			fmtimer.timera = (value << 2) + (fmtimer.timera & 3);
			break;

		case 0x25:
			fmtimer.timera = (fmtimer.timera & 0x3fc) + (value & 3);
			break;

		case 0x26:
			fmtimer.timerb = value;
			break;

		case 0x27:
			fmtimer.reg = value;
			fmtimer.status &= ~((value & 0x30) >> 4);
			if (value & 0x01) {
				if (!nevent_iswork(NEVENT_FMTIMERA)) {
					set_fmtimeraevent(NEVENT_ABSOLUTE);
				}
			}
			else {
				nevent_reset(NEVENT_FMTIMERA);
			}

			if (value & 0x02) {
				if (!nevent_iswork(NEVENT_FMTIMERB)) {
					set_fmtimerbevent(NEVENT_ABSOLUTE);
				}
			}
			else {
				nevent_reset(NEVENT_FMTIMERB);
			}

			if (!(value & 0x03)) {
				pic_resetirq(fmtimer.irq);
			}
			break;
	}
}

