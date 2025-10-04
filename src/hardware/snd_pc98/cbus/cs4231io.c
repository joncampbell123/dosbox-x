#include	"compiler.h"
#include	"cpucore.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"cs4231io.h"
#include	"sound.h"
#include	"fmboard.h"


static const UINT8 cs4231dma[] = {0xff,0x00,0x01,0x03,0xff,0xff,0xff,0xff};
static const UINT8 cs4231irq[] = {0xff,0x03,0x06,0x0a,0x0c,0xff,0xff,0xff};


static void IOOUTCALL csctrl_oc24(UINT port, REG8 dat) {

	cs4231.portctrl = dat;
	(void)port;
}

static void IOOUTCALL csctrl_oc2b(UINT port, REG8 dat) {

	UINT	num;

	if ((cs4231.portctrl & 0x28) == 0x20) {
		num = cs4231.portctrl & 7;
		if ((num == 0) || (num == 5)) {
			cs4231.port[num] &= 0xff00;
			cs4231.port[num] |= dat;
		}
	}
	(void)port;
}

static void IOOUTCALL csctrl_oc2d(UINT port, REG8 dat) {

	UINT	num;

	if ((cs4231.portctrl & 0x2f) == 0x20) {
		num = cs4231.portctrl & 7;
		if ((num == 0) || (num == 5)) {
			cs4231.port[num] &= 0x00ff;
			cs4231.port[num] |= (dat << 8);
		}
	}
	(void)port;
}

static REG8 IOINPCALL csctrl_ic24(UINT port) {

	(void)port;
	return(0x80 | cs4231.portctrl);
}

static REG8 IOINPCALL csctrl_ic2b(UINT port) {

	UINT	num;

	(void)port;
	num = cs4231.portctrl & 0x07;
	return((REG8)(cs4231.port[num] & 0xff));
}

static REG8 IOINPCALL csctrl_ic2d(UINT port) {

	UINT	num;

	(void)port;
	num = cs4231.portctrl & 0x07;
	return((REG8)(cs4231.port[num] >> 8));
}


// ----

void cs4231io_reset(void) {

	cs4231.enable = 1;
	cs4231.adrs = 0x22;
	cs4231.dmairq = cs4231irq[(cs4231.adrs >> 3) & 7];
	cs4231.dmach = cs4231dma[cs4231.adrs & 7];
	if (cs4231.dmach != 0xff) {
		dmac_attach(DMADEV_CS4231, cs4231.dmach);
	}
	cs4231.port[0] = 0x0f40;
	cs4231.port[1] = 0xa460;
	cs4231.port[2] = 0x0f48;
	cs4231.port[4] = 0x0188;
	cs4231.port[5] = 0x0f4a;

	TRACEOUT(("CS4231 - IRQ = %d", cs4231.dmairq));
	TRACEOUT(("CS4231 - DMA channel = %d", cs4231.dmach));
}

void cs4231io_bind(void) {

	sound_streamregist(&cs4231, (SOUNDCB)cs4231_getpcm);
	iocore_attachout(0xc24, csctrl_oc24);
	iocore_attachout(0xc2b, csctrl_oc2b);
	iocore_attachout(0xc2d, csctrl_oc2d);
	iocore_attachinp(0xc24, csctrl_ic24);
	iocore_attachinp(0xc2b, csctrl_ic2b);
	iocore_attachinp(0xc2d, csctrl_ic2d);
}

void IOOUTCALL cs4231io0_w8(UINT port, REG8 value) {

	switch(port - cs4231.port[0]) {
		case 0x00:
			cs4231.adrs = value;
			cs4231.dmairq = cs4231irq[(value >> 3) & 7];
			cs4231.dmach = cs4231dma[value & 7];
			dmac_detach(DMADEV_CS4231);
			if (cs4231.dmach != 0xff) {
				dmac_attach(DMADEV_CS4231, cs4231.dmach);
#if 0
				if (cs4231.sdc_enable) {
					dmac.dmach[cs4231.dmach].ready = 1;
					dmac_check();
				}
#endif
			}
			break;

		case 0x04:
			cs4231.index = value;
			break;

		case 0x05:
			cs4231_control(cs4231.index & 0x1f, value);
			break;

		case 0x06:
			cs4231.intflag = 0;
			break;

		case 0x07:
			cs4231_datasend(value);
			break;
	}
}

REG8 IOINPCALL cs4231io0_r8(UINT port) {

	switch(port - cs4231.port[0]) {
		case 0x00:
			return(cs4231.adrs);

		case 0x03:
			return(0x04);

		case 0x04:
			return(cs4231.index & 0x7f);

		case 0x05:
			return(*(((UINT8 *)(&cs4231.reg)) + (cs4231.index & 0x1f)));

		case 0x06:
			return(cs4231.intflag);

	}
	return(0);
}

void IOOUTCALL cs4231io5_w8(UINT port, REG8 value) {

	switch(port - cs4231.port[5]) {
		case 0x00:
			cs4231.extindex = value;
			break;
	}
}

REG8 IOINPCALL cs4231io5_r8(UINT port) {

	switch(port - cs4231.port[5]) {
		case 0x00:
			return(cs4231.extindex);

		case 0x01:
			if (cs4231.extindex == 1) {
				return(0);				// means opna int5
			}
			break;
	}
	return(0xff);
}

