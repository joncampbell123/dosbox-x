#include	"compiler.h"

#if defined(SUPPORT_KEYDISP)

#include	"pccore.h"
#include	"iocore.h"
#include	"sound.h"
#include	"fmboard.h"
#include	"keydisp.h"

typedef struct {
	UINT16	posx;
	UINT16	pals;
const UINT8	*data;
} KDKEYPOS;

typedef struct {
	UINT8	k[KEYDISP_NOTEMAX];
	UINT8	r[KEYDISP_NOTEMAX];
	UINT	remain;
	UINT8	flag;
	UINT8	padding[3];
} KDCHANNEL;

typedef struct {
	UINT8	ch;
	UINT8	key;
} KDDELAYE;

typedef struct {
	UINT	pos;
	UINT	rem;
	UINT8	warm;
	UINT8	warmbase;
} KDDELAY;

typedef struct {
	UINT16	fnum[4];
	UINT8	lastnote[4];
	UINT8	flag;
	UINT8	extflag;
} KDFMCTRL;

typedef struct {
	UINT16	fto[4];
	UINT8	lastnote[4];
	UINT8	flag;
	UINT8	mix;
	UINT8	padding[2];
} KDPSGCTRL;

typedef struct {
	UINT8		mode;
	UINT8		dispflag;
	UINT8		framepast;
	UINT8		keymax;
	UINT8		fmmax;
	UINT8		psgmax;
	UINT8		fmpos[KEYDISP_FMCHMAX];
	UINT8		psgpos[KEYDISP_PSGMAX];
	const UINT8	*pfmreg[KEYDISP_FMCHMAX];
	KDDELAY		delay;
	KDCHANNEL	ch[KEYDISP_CHMAX];
	KDFMCTRL	fmctl[KEYDISP_FMCHMAX];
	KDPSGCTRL	psgctl[KEYDISP_PSGMAX];
	UINT8		pal8[KEYDISP_PALS];
	UINT16		pal16[KEYDISP_LEVEL*2];
	RGB32		pal32[KEYDISP_LEVEL*2];
	KDKEYPOS	keypos[128];
	KDDELAYE	delaye[KEYDISP_DELAYEVENTS];
} KEYDISP;

static	KEYDISP		keydisp;

#include	"keydisp.res"


// ---- event

static void keyon(UINT ch, UINT8 note) {

	UINT		i;
	KDCHANNEL	*kdch;

	note &= 0x7f;
	kdch = keydisp.ch + ch;
	for (i=0; i<kdch->remain; i++) {
		if (kdch->k[i] == note) {				// ƒqƒbƒg‚µ‚½
			for (; i<(kdch->remain-1); i++) {
				kdch->k[i] = kdch->k[i+1];
				kdch->r[i] = kdch->r[i+1];
			}
			kdch->k[i] = note;
			kdch->r[i] = 0x80 | (KEYDISP_LEVEL - 1);
			kdch->flag |= 1;
			return;
		}
	}
	if (i < KEYDISP_NOTEMAX) {
		kdch->k[i] = note;
		kdch->r[i] = 0x80 | (KEYDISP_LEVEL - 1);
		kdch->flag |= 1;
		kdch->remain++;
	}
}

static void keyoff(UINT ch, UINT8 note) {

	UINT		i;
	KDCHANNEL	*kdch;

	note &= 0x7f;
	kdch = keydisp.ch + ch;
	for (i=0; i<kdch->remain; i++) {
		if (kdch->k[i] == note) {				// ƒqƒbƒg‚µ‚½
			kdch->r[i] = 0x80 | (KEYDISP_LEVEL - 2);
			kdch->flag |= 1;
			break;
		}
	}
}

static void chkeyoff(UINT ch) {

	UINT		i;
	KDCHANNEL	*kdch;

	kdch = keydisp.ch + ch;
	for (i=0; i<kdch->remain; i++) {
		if ((kdch->r[i] & (~0x80)) >= (KEYDISP_LEVEL - 1)) {
			kdch->r[i] = 0x80 | (KEYDISP_LEVEL - 2);
			kdch->flag |= 1;
		}
	}
}

static void keyalloff(void) {

	UINT8	i;

	for (i=0; i<KEYDISP_CHMAX; i++) {
		chkeyoff(i);
	}
}

static void keyallreload(void) {

	UINT	i;

	for (i=0; i<KEYDISP_CHMAX; i++) {
		keydisp.ch[i].flag = 2;
	}
}

static void keyallclear(void) {

	ZeroMemory(keydisp.ch, sizeof(keydisp.ch));
	keyallreload();
}


// ---- delay event

static void delayreset(void) {

	keydisp.delay.warm = keydisp.delay.warmbase;
	keydisp.delay.pos = 0;
	keydisp.delay.rem = 0;
	ZeroMemory(keydisp.delaye, sizeof(keydisp.delaye));
	keyalloff();
}

static void delayexecevent(UINT8 framepast) {

	KDDELAYE	*ebase;
	UINT		pos;
	UINT		rem;
	KDDELAYE	*ev;

	ebase = keydisp.delaye;
	pos = keydisp.delay.pos;
	rem = keydisp.delay.rem;
	while((keydisp.delay.warm) && (framepast)) {
		keydisp.delay.warm--;
		framepast--;
		if (rem >= KEYDISP_DELAYEVENTS) {
			ev = ebase + pos;
			rem--;
			if (ev->ch == 0xff) {
				keydisp.delay.warm++;
			}
			else if (ev->key & 0x80) {
				keyon(ev->ch, ev->key);
				rem--;
			}
			else {
				keyoff(ev->ch, ev->key);
			}
			pos = (pos + 1) & (KEYDISP_DELAYEVENTS - 1);
		}
		ebase[(pos + rem) & (KEYDISP_DELAYEVENTS - 1)].ch = 0xff;
		rem++;
	}
	while(framepast) {
		framepast--;
		while(rem) {
			rem--;
			ev = ebase + pos;
			if (ev->ch == 0xff) {
				pos = (pos + 1) & (KEYDISP_DELAYEVENTS - 1);
				break;
			}
			if (ev->key & 0x80) {
				keyon(ev->ch, ev->key);
			}
			else {
				keyoff(ev->ch, ev->key);
			}
			pos = (pos + 1) & (KEYDISP_DELAYEVENTS - 1);
		}
		ebase[(pos + rem) & (KEYDISP_DELAYEVENTS - 1)].ch = 0xff;
		rem++;
	}
	keydisp.delay.pos = pos;
	keydisp.delay.rem = rem;
}

static void delaysetevent(UINT8 ch, UINT8 key) {

	KDDELAYE	*e;

	e = keydisp.delaye;
	if (keydisp.delay.rem < KEYDISP_DELAYEVENTS) {
		e += (keydisp.delay.pos + keydisp.delay.rem) &
													(KEYDISP_DELAYEVENTS - 1);
		keydisp.delay.rem++;
		e->ch = ch;
		e->key = key;
	}
	else {
		e += keydisp.delay.pos;
		keydisp.delay.pos += (keydisp.delay.pos + 1) &
													(KEYDISP_DELAYEVENTS - 1);
		if (e->ch == 0xff) {
			keydisp.delay.warm++;
		}
		else if (e->key & 0x80) {
			keyon(e->ch, e->key);
		}
		else {
			keyoff(e->ch, e->key);
		}
		e->ch = ch;
		e->key = key;
	}
}


// ---- FM

static UINT8 getfmnote(UINT16 fnum) {

	UINT8	ret;
	int		i;

	ret = (fnum >> 11) & 7;
	ret *= 12;
	ret += 24;
	fnum &= 0x7ff;

	while(fnum < FNUM_MIN) {
		if (!ret) {
			return(0);
		}
		ret -= 12;
		fnum <<= 1;
	}
	while(fnum > FNUM_MAX) {
		fnum >>= 1;
		ret += 12;
	}
	for (i=0; fnum >= fnumtbl[i]; i++) {
		ret++;
	}
	if (ret > 127) {
		return(127);
	}
	return(ret);
}

static void fmkeyoff(UINT8 ch, KDFMCTRL *k) {

	delaysetevent(keydisp.fmpos[ch], k->lastnote[0]);
}

static void fmkeyon(UINT8 ch, KDFMCTRL *k) {

	const UINT8 *pReg;

	fmkeyoff(ch, k);
	pReg = keydisp.pfmreg[ch];
	if (pReg)
	{
		pReg = pReg + 0xa0;
		k->fnum[0] = ((pReg[4] & 0x3f) << 8) + pReg[0];
		k->lastnote[0] = getfmnote(k->fnum[0]);
		delaysetevent(keydisp.fmpos[ch], (UINT8)(k->lastnote[0] | 0x80));
	}
}

static void fmkeyreset(void) {

	ZeroMemory(keydisp.fmctl, sizeof(keydisp.fmctl));
}

void keydisp_fmkeyon(UINT8 ch, UINT8 value) {

	KDFMCTRL	*k;

	if (keydisp.mode != KEYDISP_MODEFM) {
		return;
	}
	if (ch < keydisp.fmmax) {
		k = keydisp.fmctl + ch;
		value &= 0xf0;
		if (k->flag != value) {
			if (value) {
				fmkeyon(ch, k);
			}
			else {
				fmkeyoff(ch, k);
			}
			k->flag = value;
		}
	}
}

static void fmkeysync(void) {

	UINT8		ch;
	KDFMCTRL	*k;
	const UINT8 *pReg;
	UINT16		fnum;

	for (ch=0, k=keydisp.fmctl; ch<keydisp.fmmax; ch++, k++) {
		if (k->flag) {
			pReg = keydisp.pfmreg[ch];
			if (pReg)
			{
				pReg = pReg + 0xa0;
				fnum = ((pReg[4] & 0x3f) << 8) + pReg[0];
				if (k->fnum[0] != fnum) {
					UINT8 n;
					k->fnum[0] = fnum;
					n = getfmnote(fnum);
					if (k->lastnote[0] != n) {
						fmkeyoff(ch, k);
					}
					k->lastnote[0] = n;
					delaysetevent(keydisp.fmpos[ch],
											(UINT8)(k->lastnote[0] | 0x80));
				}
			}
		}
	}
}


// ---- PSG

static const void *psgtbl[3] = {&psg1, &psg2, &psg3};

static UINT8 getpsgnote(UINT16 tone) {

	UINT8	ret;
	int		i;

	ret = 60;
	tone &= 0xfff;

	while(tone < FTO_MIN) {
		tone <<= 1;
		ret += 12;
		if (ret >= 128) {
			return(127);
		}
	}
	while(tone > FTO_MAX) {
		if (!ret) {
			return(0);
		}
		tone >>= 1;
		ret -= 12;
	}
	for (i=0; tone < ftotbl[i]; i++) {
		ret++;
	}
	if (ret >= 128) {
		return(127);
	}
	return(ret);
}

static void psgmix(UINT8 ch, PSGGEN psg) {

	KDPSGCTRL	*k;

	k = keydisp.psgctl + ch;
	if ((k->mix ^ psg->reg.mixer) & 7) {
		UINT8 i, bit, pos;
		k->mix = psg->reg.mixer;
		pos = keydisp.psgpos[ch];
		for (i=0, bit=1; i<3; i++, pos++, bit<<=1) {
			if (k->flag & bit) {
				k->flag ^= bit;
				delaysetevent(pos, k->lastnote[i]);
			}
			else if ((!(k->mix & bit)) && (psg->reg.vol[i] & 0x1f)) {
				k->flag |= bit;
				k->fto[i] = LOADINTELWORD(psg->reg.tune[i]) & 0xfff;
				k->lastnote[i] = getpsgnote(k->fto[i]);
				delaysetevent(pos, (UINT8)(k->lastnote[i] | 0x80));
			}
		}
	}
}

static void psgvol(UINT8 ch, PSGGEN psg, UINT8 i) {

	KDPSGCTRL	*k;
	UINT8		bit;
	UINT8		pos;
	UINT16		tune;

	k = keydisp.psgctl + ch;
	bit = (1 << i);
	pos = keydisp.psgpos[ch] + i;
	if (psg->reg.vol[i] & 0x1f) {
		if (!((k->mix | k->flag) & bit)) {
			k->flag |= bit;
			tune = LOADINTELWORD(psg->reg.tune[i]);
			tune &= 0xfff;
			k->fto[i] = tune;
			k->lastnote[i] = getpsgnote(tune);
			delaysetevent(pos, (UINT8)(k->lastnote[i] | 0x80));
		}
	}
	else if (k->flag & bit) {
		k->flag ^= bit;
		delaysetevent(pos, k->lastnote[i]);
	}
}

static void psgkeyreset(void) {

	ZeroMemory(keydisp.psgctl, sizeof(keydisp.psgctl));
}

void keydisp_psgmix(void *psg) {

	UINT8	c;

	if (keydisp.mode != KEYDISP_MODEFM) {
		return;
	}
	for (c=0; c<keydisp.psgmax; c++) {
		if (psgtbl[c] == psg) {
			psgmix(c, (PSGGEN)psg);
			break;
		}
	}
}

void keydisp_psgvol(void *psg, UINT8 ch) {

	UINT8	c;

	if (keydisp.mode != KEYDISP_MODEFM) {
		return;
	}
	for (c=0; c<keydisp.psgmax; c++) {
		if (psgtbl[c] == psg) {
			psgvol(c, (PSGGEN)psg, ch);
			break;
		}
	}
}

static void psgkeysync(void) {

	UINT8		ch;
	KDPSGCTRL	*k;
	UINT8		bit;
	UINT8		i;
	UINT8		pos;
	PSGGEN		psg;
	UINT16		tune;
	UINT8		n;

	for (ch=0, k=keydisp.psgctl; ch<keydisp.psgmax; ch++, k++) {
		psg = (PSGGEN)psgtbl[ch];
		pos = keydisp.psgpos[ch];
		for (i=0, bit=1; i<3; i++, pos++, bit<<=1) {
			if (k->flag & bit) {
				tune = LOADINTELWORD(psg->reg.tune[i]);
				tune &= 0xfff;
				if (k->fto[i] != tune) {
					k->fto[i] = tune;
					n = getpsgnote(tune);
					if (k->lastnote[i] != n) {
						delaysetevent(pos, k->lastnote[i]);
						k->lastnote[i] = n;
						delaysetevent(pos, (UINT8)(n | 0x80));
					}
				}
			}
		}
	}
}


// ---- BOARD change...

static void setfmhdl(UINT8 items, UINT base) {

	while(items--) {
		if ((keydisp.keymax < KEYDISP_CHMAX) &&
			(keydisp.fmmax < KEYDISP_FMCHMAX)) {
			keydisp.fmpos[keydisp.fmmax] = keydisp.keymax++;
			keydisp.pfmreg[keydisp.fmmax] = opn.reg + base;
			keydisp.fmmax++;
			base++;
			if ((base & 3) == 3) {
				base += 0x100 - 3;
			}
		}
	}
}

#if defined(SUPPORT_PX)
static void setfmhdlex(const OPN_T *pOpn, UINT nItems, UINT nBase) {

	while(nItems--) {
		if ((keydisp.keymax < KEYDISP_CHMAX) &&
			(keydisp.fmmax < KEYDISP_FMCHMAX)) {
			keydisp.fmpos[keydisp.fmmax] = keydisp.keymax++;
			keydisp.pfmreg[keydisp.fmmax] = pOpn->reg + nBase;
			keydisp.fmmax++;
			nBase++;
			if ((nBase & 3) == 3) {
				nBase += 0x100 - 3;
			}
		}
	}
}
#endif	// defined(SUPPORT_PX)

static void setpsghdl(UINT8 items) {

	while(items--) {
		if ((keydisp.keymax <= (KEYDISP_CHMAX - 3)) &&
			(keydisp.psgmax < KEYDISP_PSGMAX)) {
			keydisp.psgpos[keydisp.psgmax++] = keydisp.keymax;
			keydisp.keymax += 3;
		}
	}
}

void keydisp_setfmboard(UINT b) {

	keydisp.keymax = 0;
	keydisp.fmmax = 0;
	keydisp.psgmax = 0;

#if defined(SUPPORT_PX)
	if (b == 0x30)
	{
		setfmhdlex(&opn, 12, 0);
		setfmhdlex(&opn2, 12, 0);
		setpsghdl(2);
		b = 0;
	}
	if (b == 0x50)
	{
		setfmhdlex(&opn, 12, 0);
		setfmhdlex(&opn2, 12, 0);
		setfmhdlex(&opn3, 6, 0);
		setpsghdl(3);
		b = 0;
	}

#endif	// defined(SUPPORT_PX)

	if (b & 0x02) {
		if (!(b & 0x04)) {
			setfmhdl(3, 0);
		}
		else {								// ‚Q–‡Žh‚µ‚ÌŽžƒŒƒWƒXƒ^ˆÚ“®
			setfmhdl(3, 0x200);
		}
		setpsghdl(1);
	}
	if (b & 0x04) {
		setfmhdl(6, 0);
		setpsghdl(1);
	}
	if (b & 0x08) {
		setfmhdl(6, 0);
		setpsghdl(1);
	}
	if (b & 0x20) {
		setfmhdl(6, 0);
		setpsghdl(1);
	}
	if (b & 0x40) {
		setfmhdl(12, 0);
		setpsghdl(1);
	}
	if (b & 0x80) {
		setpsghdl(3);
	}
	delayreset();
	fmkeyreset();
	psgkeyreset();

	if (keydisp.mode == KEYDISP_MODEFM) {
		keydisp.dispflag |= KEYDISP_FLAGSIZING;
	}
}


// ---- MIDI

void keydisp_midi(const UINT8 *cmd) {

	if (keydisp.mode != KEYDISP_MODEMIDI) {
		return;
	}
	switch(cmd[0] & 0xf0) {
		case 0x80:
			keyoff(cmd[0] & 0x0f, cmd[1]);
			break;

		case 0x90:
			if (cmd[2] & 0x7f) {
				keyon(cmd[0] & 0x0f, cmd[1]);
			}
			else {
				keyoff(cmd[0] & 0x0f, cmd[1]);
			}
			break;

		case 0xb0:
			if ((cmd[1] == 0x78) || (cmd[1] == 0x79) || (cmd[1] == 0x7b)) {
				chkeyoff(cmd[0] & 0x0f);
			}
			break;

		case 0xfe:
			keyalloff();
			break;
	}
}


// ---- draw

static int getdispkeys(void) {

	int		keys;

	switch(keydisp.mode) {
		case KEYDISP_MODEFM:
			keys = keydisp.keymax;
			break;

		case KEYDISP_MODEMIDI:
			keys = 16;
			break;

		default:
			keys = 0;
			break;
	}
	if (keys > KEYDISP_CHMAX) {
		keys = KEYDISP_CHMAX;
	}
	return(keys);
}

static void clearrect(CMNVRAM *vram, int x, int y, int cx, int cy) {

	CMNPAL	col;

	switch(vram->bpp) {
#if defined(SUPPORT_8BPP)
		case 8:
			col.pal8 = keydisp.pal8[KEYDISP_PALBG];
			break;
#endif
#if defined(SUPPORT_16BPP)
		case 16:
			col.pal16 = keydisp.pal16[KEYDISP_LEVEL];
			break;
#endif
#if defined(SUPPORT_24BPP)
		case 24:
#endif
#if defined(SUPPORT_32BPP)
		case 32:
#endif
#if defined(SUPPORT_24BPP) || defined(SUPPORT_32BPP)
			col.pal32 = keydisp.pal32[KEYDISP_LEVEL];
			break;
#endif
		default:
			return;
	}
	cmndraw_fill(vram, x, y, cx, cy, col);
}

static void drawkeybg(CMNVRAM *vram) {

	CMNPAL	bg;
	CMNPAL	fg;
	int		i;

	switch(vram->bpp) {
#if defined(SUPPORT_8BPP)
		case 8:
			bg.pal8 = keydisp.pal8[KEYDISP_PALBG];
			fg.pal8 = keydisp.pal8[KEYDISP_PALFG];
			break;
#endif
#if defined(SUPPORT_16BPP)
		case 16:
			bg.pal16 = keydisp.pal16[KEYDISP_LEVEL];
			fg.pal16 = keydisp.pal16[0];
			break;
#endif
#if defined(SUPPORT_24BPP)
		case 24:
			bg.pal32 = keydisp.pal32[KEYDISP_LEVEL];
			fg.pal32 = keydisp.pal32[0];
			break;
#endif
#if defined(SUPPORT_32BPP)
		case 32:
			bg.pal32 = keydisp.pal32[KEYDISP_LEVEL];
			fg.pal32 = keydisp.pal32[0];
			break;
#endif
		default:
			return;
	}
	for (i=0; i<10; i++) {
		cmndraw_setpat(vram, keybrd1, i * KEYDISP_KEYCX, 0, bg, fg);
	}
	cmndraw_setpat(vram, keybrd2, 10 * KEYDISP_KEYCX, 0, bg, fg);
}

static BOOL draw1key(CMNVRAM *vram, KDCHANNEL *kdch, UINT n) {

	KDKEYPOS	*pos;
	UINT		pal;
	CMNPAL		fg;

	pos = keydisp.keypos + (kdch->k[n] & 0x7f);
	pal = kdch->r[n] & 0x7f;
	switch(vram->bpp) {
#if defined(SUPPORT_8BPP)
		case 8:
			if (pal != (KEYDISP_LEVEL - 1)) {
				fg.pal8 = keydisp.pal8[
									(pos->pals)?KEYDISP_PALBG:KEYDISP_PALFG];
				cmndraw_setfg(vram, pos->data, pos->posx, 0, fg);
				kdch->r[n] = 0;
				return(TRUE);
			}
			fg.pal8 = keydisp.pal8[KEYDISP_PALHIT];
			break;
#endif
#if defined(SUPPORT_16BPP)
		case 16:
			fg.pal16 = keydisp.pal16[pal + pos->pals];
			break;
#endif
#if defined(SUPPORT_24BPP)
		case 24:
			fg.pal32 = keydisp.pal32[pal + pos->pals];
			break;
#endif
#if defined(SUPPORT_32BPP)
		case 32:
			fg.pal32 = keydisp.pal32[pal + pos->pals];
			break;
#endif
		default:
			return(FALSE);
	}
	cmndraw_setfg(vram, pos->data, pos->posx, 0, fg);
	return(FALSE);
}

static BOOL draw1ch(CMNVRAM *vram, UINT8 framepast, KDCHANNEL *kdch) {

	BOOL	draw;
	UINT	i;
	BOOL	coll;
	UINT8	nextf;
	UINT	j;

	draw = FALSE;
	if (kdch->flag & 2) {
		drawkeybg(vram);
		draw = TRUE;
	}
	if (kdch->flag) {
		coll = FALSE;
		nextf = 0;
		for (i=0; i<kdch->remain; i++) {
			if ((kdch->r[i] & 0x80) || (kdch->flag & 2)) {
				kdch->r[i] &= ~0x80;
				if (kdch->r[i] < (KEYDISP_LEVEL - 1)) {
					if (kdch->r[i] > framepast) {
						kdch->r[i] -= framepast;
						kdch->r[i] |= 0x80;
						nextf = 1;
					}
					else {
						kdch->r[i] = 0;
						coll = TRUE;
					}
				}
				coll |= draw1key(vram, kdch, i);
				draw = TRUE;
			}
		}
		if (coll) {
			for (i=0; i<kdch->remain; i++) {
				if (!kdch->r[i]) {
					break;
				}
			}
			for (j=i; i<kdch->remain; i++) {
				if (kdch->r[i]) {
					kdch->k[j] = kdch->k[i];
					kdch->r[j] = kdch->r[i];
					j++;
				}
			}
			kdch->remain = j;
		}
		kdch->flag = nextf;
	}
	return(draw);
}


// ----

void keydisp_initialize(void) {

	int		r;
	UINT16	x;
	int		i;

	r = 0;
	x = 0;
	do {
		for (i=0; i<12 && r<128; i++, r++) {
			keydisp.keypos[r].posx = keyposdef[i].posx + x;
			keydisp.keypos[r].pals = keyposdef[i].pals;
			keydisp.keypos[r].data = keyposdef[i].data;
		}
		x += 28;
	} while(r < 128);
	keyallclear();
}

void keydisp_setpal(CMNPALFN *palfn) {

	UINT	i;
	RGB32	pal32[KEYDISP_PALS];

	if (palfn == NULL) {
		return;
	}
	if (palfn->get8) {
		for (i=0; i<KEYDISP_PALS; i++) {
			keydisp.pal8[i] = (*palfn->get8)(palfn, i);
		}
	}
	if (palfn->get32) {
		for (i=0; i<KEYDISP_PALS; i++) {
			pal32[i].d = (*palfn->get32)(palfn, i);
			cmndraw_makegrad(keydisp.pal32, KEYDISP_LEVEL,
								pal32[KEYDISP_PALFG], pal32[KEYDISP_PALHIT]);
			cmndraw_makegrad(keydisp.pal32 + KEYDISP_LEVEL, KEYDISP_LEVEL,
								pal32[KEYDISP_PALBG], pal32[KEYDISP_PALHIT]);
		}
		if (palfn->cnv16) {
			for (i=0; i<KEYDISP_LEVEL*2; i++) {
				keydisp.pal16[i] = (*palfn->cnv16)(palfn, keydisp.pal32[i]);
			}
		}
	}
	keydisp.dispflag |= KEYDISP_FLAGREDRAW;
}

void keydisp_setmode(UINT8 mode) {

	if (keydisp.mode != mode) {
		keydisp.mode = mode;
		keydisp.dispflag |= KEYDISP_FLAGREDRAW | KEYDISP_FLAGSIZING;
		keyallclear();
		if (mode == KEYDISP_MODEFM) {
			delayreset();
			fmkeyreset();
			psgkeyreset();
		}
	}
	else {
		keyalloff();
	}
}

void keydisp_setdelay(UINT8 frames) {

	keydisp.delay.warmbase = frames;
	delayreset();
}

UINT8 keydisp_process(UINT8 framepast) {

	int		keys;
	int		i;

	if (framepast) {
		if (keydisp.mode == KEYDISP_MODEFM) {
			fmkeysync();
			psgkeysync();
			delayexecevent(framepast);
		}
		keydisp.framepast += framepast;
	}
	keys = getdispkeys();
	for (i=0; i<keys; i++) {
		if (keydisp.ch[i].flag) {
			keydisp.dispflag |= KEYDISP_FLAGDRAW;
			break;
		}
	}
	return(keydisp.dispflag);
}

void keydisp_getsize(int *width, int *height) {

	if (width) {
		*width = KEYDISP_WIDTH;
	}
	if (height) {
		*height = (getdispkeys() * KEYDISP_KEYCY) + 1;
	}
	keydisp.dispflag &= ~KEYDISP_FLAGSIZING;
}

BOOL keydisp_paint(CMNVRAM *vram, BOOL redraw) {

	BOOL		draw;
	int			keys;
	int			i;
	KDCHANNEL	*p;

	draw = FALSE;
	if ((vram == NULL) ||
		(vram->width < KEYDISP_WIDTH) || (vram->height < 1)) {
		goto kdpnt_exit;
	}
	if (keydisp.dispflag & KEYDISP_FLAGREDRAW){
		redraw = TRUE;
	}
	if (redraw) {
		keyallreload();
		clearrect(vram, 0, 0, KEYDISP_WIDTH, 1);
		clearrect(vram, 0, 0, 1, vram->height);
		draw = TRUE;
	}
	vram->ptr += vram->xalign + vram->yalign;		// ptr (1, 1)
	keys = (vram->height - 1) / KEYDISP_KEYCY;
	keys = min(keys, getdispkeys());
	for (i=0, p=keydisp.ch; i<keys; i++, p++) {
		draw |= draw1ch(vram, keydisp.framepast, p);
		vram->ptr += KEYDISP_KEYCY * vram->yalign;
	}
	keydisp.dispflag &= ~(KEYDISP_FLAGDRAW | KEYDISP_FLAGREDRAW);
	keydisp.framepast = 0;

kdpnt_exit:
	return(draw);
}
#endif

