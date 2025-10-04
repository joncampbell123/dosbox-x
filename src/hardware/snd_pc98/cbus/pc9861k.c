#include	"compiler.h"

#if defined(SUPPORT_PC9861K)

#include	"commng.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"cbuscore.h"
#include	"pc9861k.h"


	_PC9861K	pc9861k;
	COMMNG		cm_pc9861ch1;
	COMMNG		cm_pc9861ch2;


const UINT32 pc9861k_speed[11] = {75, 150, 300, 600, 1200, 2400, 4800,
											9600, 19200, 38400, 76800};

static const UINT8 ch1_irq[4] = {IRQ_INT0, IRQ_INT1, IRQ_INT2, IRQ_INT3};
static const UINT8 ch2_irq[4] = {IRQ_INT0, IRQ_INT41, IRQ_INT5, IRQ_INT6};
static const _PC9861CH pc9861def =
					{0x05, 0xff, 7, 1,
						0, 0, 2400, NEVENT_MAXCLOCK, 0, 0, 0};


// ----

static void pc9861k_callback(COMMNG cm, PC9861CH m) {

	BOOL	interrupt;

	interrupt = FALSE;
	if ((cm != NULL) && (cm->read(cm, &m->data))) {
		m->result |= 2;
		if (m->signal & 1) {
			interrupt = TRUE;
		}
	}
	else {
		m->result &= ~2;
	}
	if (m->signal & 4) {
		if (m->send) {
			m->send = 0;
			interrupt = TRUE;
		}
	}
	if (interrupt) {
		pic_setirq(m->irq);
	}
}

void pc9861ch1cb(NEVENTITEM item) {

	if (item->flag & NEVENT_SETEVENT) {
		nevent_set(NEVENT_PC9861CH1, pc9861k.ch1.clk, pc9861ch1cb,
														NEVENT_RELATIVE);
	}
	pc9861k_callback(cm_pc9861ch1, &pc9861k.ch1);
}

void pc9861ch2cb(NEVENTITEM item) {

	if (item->flag & NEVENT_SETEVENT) {
		nevent_set(NEVENT_PC9861CH2, pc9861k.ch2.clk, pc9861ch2cb,
														NEVENT_RELATIVE);
	}
	pc9861k_callback(cm_pc9861ch2, &pc9861k.ch2);
}

static UINT32 pc9861k_getspeed(REG8 dip) {

	UINT	speed;

	speed = (((~dip) >> 2) & 0x0f) + 1;
	if (dip & 0x02) {
		if (speed > 4) {
			speed -= 4;
		}
		else {
			speed = 0;
		}
	}
	if (speed > (NELEMENTS(pc9861k_speed) - 1)) {
		speed = NELEMENTS(pc9861k_speed) - 1;
	}
	return(pc9861k_speed[speed]);
}

static void pc9861_makeclk(PC9861CH m, UINT32 mul2) {

	m->clk = pccore.realclock * mul2 / ((m->speed) * 2);
}

static void pc9861ch1_open(void) {

	cm_pc9861ch1 = commng_create(COMCREATE_PC9861K1);

	pc9861k.ch1.dip = np2cfg.pc9861sw[0];
	pc9861k.ch1.speed = pc9861k_getspeed(pc9861k.ch1.dip);
	pc9861k.ch1.vect = ((np2cfg.pc9861sw[1] >> 1) & 1) |
						((np2cfg.pc9861sw[1] << 1) & 2);
	pc9861k.ch1.irq = ch1_irq[pc9861k.ch1.vect];
	pc9861_makeclk(&pc9861k.ch1, 10*2);
	nevent_set(NEVENT_PC9861CH1, pc9861k.ch1.clk,
											pc9861ch1cb, NEVENT_ABSOLUTE);
}

static void pc9861ch2_open(void) {

	cm_pc9861ch2 = commng_create(COMCREATE_PC9861K2);

	pc9861k.ch2.dip = np2cfg.pc9861sw[2];
	pc9861k.ch2.speed = pc9861k_getspeed(pc9861k.ch2.dip);
	pc9861k.ch2.vect = ((np2cfg.pc9861sw[1] >> 3) & 1) |
						((np2cfg.pc9861sw[1] >> 1) & 2);
	pc9861k.ch2.irq = ch1_irq[pc9861k.ch2.vect];
	pc9861_makeclk(&pc9861k.ch2, 10*2);
	nevent_set(NEVENT_PC9861CH2, pc9861k.ch2.clk,
											pc9861ch2cb, NEVENT_ABSOLUTE);
}


// -------------------------------------------------------------------------

static void IOOUTCALL pc9861data_w8(COMMNG cm, PC9861CH m,
													UINT port, REG8 value) {

	UINT32	mul2;

	switch(port & 0x3) {
		case 0x01:
			cm->write(cm, (UINT8)value);
			if (m->signal & 4) {
				m->send = 0;
				pic_setirq(m->irq);
			}
			else {
				m->send = 1;
			}
			break;

		case 0x03:
			if (!(value & 0xfd)) {
				m->dummyinst += 1;
			}
			else {
				if ((m->dummyinst >= 3) && (value == 0x40)) {
					m->pos = 0;
				}
				m->dummyinst = 0;
			}
			switch(m->pos) {
				case 0x00:			// reset
					m->pos += 1;
					break;

				case 0x01:			// mode
					if (!(value & 0x03)) {
						mul2 = 10 * 2;
					}
					else {
						mul2 = ((value >> 1) & 6) + 10;
						if (value & 0x10) {
							mul2 += 2;
						}
						switch(value & 0xc0) {
							case 0x80:
								mul2 += 3;
								break;
							case 0xc0:
								mul2 += 4;
								break;
							default:
								mul2 += 2;
								break;
						}
					}
					pc9861_makeclk(m, mul2);
					m->pos += 1;
					break;

				case 0x02:			// cmd
					m->pos += 1;
					break;
			}
			break;
	}
}

static REG8 IOINPCALL pc9861data_r8(COMMNG cm, PC9861CH m, UINT port) {

	switch(port & 0x3) {
		case 0x01:
			return(m->data);

		case 0x03:
			if (!(cm->getstat(cm) & 0x20)) {
				return((m->result) | 0x80);
			}
			return(m->result);
	}
	return(0xff);
}


static void IOOUTCALL pc9861k_ob0(UINT port, REG8 dat) {

	if (cm_pc9861ch1 == NULL) {
		pc9861ch1_open();
	}
	pc9861k.ch1.signal = dat;
	(void)port;
}

static void IOOUTCALL pc9861k_ob2(UINT port, REG8 dat) {

	if (cm_pc9861ch2 == NULL) {
		pc9861ch2_open();
	}
	pc9861k.ch2.signal = dat;
	(void)port;
}

static REG8 IOINPCALL pc9861k_ib0(UINT port) {

	if (cm_pc9861ch1 == NULL) {
		pc9861ch1_open();
	}
	(void)port;
	return(cm_pc9861ch1->getstat(cm_pc9861ch1) | pc9861k.ch1.vect);
}

static REG8 IOINPCALL pc9861k_ib2(UINT port) {

	if (cm_pc9861ch2 == NULL) {
		pc9861ch2_open();
	}
	(void)port;
	return(cm_pc9861ch2->getstat(cm_pc9861ch2) | pc9861k.ch2.vect);
}


static void IOOUTCALL pc9861k_ob1(UINT port, REG8 dat) {

	if (cm_pc9861ch1 == NULL) {
		pc9861ch1_open();
	}
	pc9861data_w8(cm_pc9861ch1, &pc9861k.ch1, port, dat);
}

static REG8 IOINPCALL pc9861k_ib1(UINT port) {

	if (cm_pc9861ch2 == NULL) {
		pc9861ch1_open();
	}
	return(pc9861data_r8(cm_pc9861ch1, &pc9861k.ch1, port));
}


static void IOOUTCALL pc9861k_ob9(UINT port, REG8 dat) {

	if (cm_pc9861ch2 == NULL) {
		pc9861ch2_open();
	}
	pc9861data_w8(cm_pc9861ch2, &pc9861k.ch2, port, dat);
}

static REG8 IOINPCALL pc9861k_ib9(UINT port) {

	if (cm_pc9861ch2 == NULL) {
		pc9861ch2_open();
	}
	return(pc9861data_r8(cm_pc9861ch2, &pc9861k.ch2, port));
}


// ---- I/F

void pc9861k_initialize(void) {

	cm_pc9861ch1 = NULL;
	cm_pc9861ch2 = NULL;
}

void pc9861k_deinitialize(void) {

	commng_destroy(cm_pc9861ch1);
	cm_pc9861ch1 = NULL;
	commng_destroy(cm_pc9861ch2);
	cm_pc9861ch2 = NULL;
}

void pc9861k_reset(const NP2CFG *pConfig) {

	commng_destroy(cm_pc9861ch1);
	cm_pc9861ch1 = NULL;
	commng_destroy(cm_pc9861ch2);
	cm_pc9861ch2 = NULL;

	pc9861k.ch1 = pc9861def;
	pc9861k.ch2 = pc9861def;
	pc9861k.en = pConfig->pc9861enable & 1;
}

void pc9861k_bind(void) {

	pc9861k_deinitialize();
	if (pc9861k.en) {
		iocore_attachout(0xb0, pc9861k_ob0);
		iocore_attachout(0xb2, pc9861k_ob2);
		iocore_attachinp(0xb0, pc9861k_ib0);
		iocore_attachinp(0xb2, pc9861k_ib2);

		iocore_attachout(0xb1, pc9861k_ob1);
		iocore_attachout(0xb3, pc9861k_ob1);
		iocore_attachinp(0xb1, pc9861k_ib1);
		iocore_attachinp(0xb3, pc9861k_ib1);

		iocore_attachout(0xb9, pc9861k_ob9);
		iocore_attachout(0xbb, pc9861k_ob9);
		iocore_attachinp(0xb9, pc9861k_ib9);
		iocore_attachinp(0xbb, pc9861k_ib9);
	}
}

void pc9861k_midipanic(void) {

	if (cm_pc9861ch1) {
		cm_pc9861ch1->msg(cm_pc9861ch1, COMMSG_MIDIRESET, 0);
	}
	if (cm_pc9861ch2) {
		cm_pc9861ch2->msg(cm_pc9861ch1, COMMSG_MIDIRESET, 0);
	}
}

#endif

