#include	"compiler.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"sound.h"
#include	"fmboard.h"


#if defined(OPNGENX86)
#error use opngen.x86
#endif


extern	OPNCFG	opncfg;


#define	CALCENV(e, c, s)													\
	(c)->slot[(s)].freq_cnt += (c)->slot[(s)].freq_inc;						\
	(c)->slot[(s)].env_cnt += (c)->slot[(s)].env_inc;						\
	if ((c)->slot[(s)].env_cnt >= (c)->slot[(s)].env_end) {					\
		switch((c)->slot[(s)].env_mode) {									\
			case EM_ATTACK:													\
				(c)->slot[(s)].env_mode = EM_DECAY1;						\
				(c)->slot[(s)].env_cnt = EC_DECAY;							\
				(c)->slot[(s)].env_end = (c)->slot[(s)].decaylevel;			\
				(c)->slot[(s)].env_inc = (c)->slot[(s)].env_inc_decay1;		\
				break;														\
			case EM_DECAY1:													\
				(c)->slot[(s)].env_mode = EM_DECAY2;						\
				(c)->slot[(s)].env_cnt = (c)->slot[(s)].decaylevel;			\
				(c)->slot[(s)].env_end = EC_OFF;							\
				(c)->slot[(s)].env_inc = (c)->slot[(s)].env_inc_decay2;		\
				break;														\
			case EM_RELEASE:												\
				(c)->slot[(s)].env_mode = EM_OFF;							\
			case EM_DECAY2:													\
				(c)->slot[(s)].env_cnt = EC_OFF;							\
				(c)->slot[(s)].env_end = EC_OFF + 1;						\
				(c)->slot[(s)].env_inc = 0;									\
				(c)->playing &= ~(1 << (s));								\
				break;														\
		}																	\
	}																		\
	(e) = (c)->slot[(s)].totallevel -										\
					opncfg.envcurve[(c)->slot[(s)].env_cnt >> ENV_BITS];

#define SLOTOUT(s, e, c)													\
		((opncfg.sintable[(((s).freq_cnt + (c)) >>							\
							(FREQ_BITS - SIN_BITS)) & (SIN_ENT - 1)] *		\
				opncfg.envtable[(e)]) >> (ENVTBL_BIT+SINTBL_BIT-TL_BITS))


static void calcratechannel(OPNCH *ch) {

	SINT32	envout;
	SINT32	opout;

	opngen.feedback2 = 0;
	opngen.feedback3 = 0;
	opngen.feedback4 = 0;

	/* SLOT 1 */
	CALCENV(envout, ch, 0);
	if (envout > 0) {
		if (ch->feedback) {
			/* with self feed back */
			opout = ch->op1fb;
			ch->op1fb = SLOTOUT(ch->slot[0], envout,
											(ch->op1fb >> ch->feedback));
			opout = (opout + ch->op1fb) / 2;
		}
		else {
			/* without self feed back */
			opout = SLOTOUT(ch->slot[0], envout, 0);
		}
		/* output slot1 */
		if (!ch->connect1) {
			opngen.feedback2 = opngen.feedback3 = opngen.feedback4 = opout;
		}
		else {
			*ch->connect1 += opout;
		}
	}
	/* SLOT 2 */
	CALCENV(envout, ch, 1);
	if (envout > 0) {
		*ch->connect2 += SLOTOUT(ch->slot[1], envout, opngen.feedback2);
	}
	/* SLOT 3 */
	CALCENV(envout, ch, 2);
	if (envout > 0) {
		*ch->connect3 += SLOTOUT(ch->slot[2], envout, opngen.feedback3);
	}
	/* SLOT 4 */
	CALCENV(envout, ch, 3);
	if (envout > 0) {
		*ch->connect4 += SLOTOUT(ch->slot[3], envout, opngen.feedback4);
	}
}

void SOUNDCALL opngen_getpcm(void *hdl, SINT32 *pcm, UINT count) {

	OPNCH	*fm;
	UINT	i;
	UINT	playing;
	SINT32	samp_l;
	SINT32	samp_r;

	if ((!opngen.playing) || (!count)) {
		return;
	}
	fm = opnch;
	do {
		samp_l = opngen.outdl * (opngen.calcremain * -1);
		samp_r = opngen.outdr * (opngen.calcremain * -1);
		opngen.calcremain += FMDIV_ENT;
		while(1) {
			opngen.outdc = 0;
			opngen.outdl = 0;
			opngen.outdr = 0;
			playing = 0;
			for (i=0; i<opngen.playchannels; i++) {
				if (fm[i].playing & fm[i].outslot) {
					calcratechannel(fm + i);
					playing++;
				}
			}
			opngen.outdl += opngen.outdc;
			opngen.outdr += opngen.outdc;
			opngen.outdl >>= FMVOL_SFTBIT;
			opngen.outdr >>= FMVOL_SFTBIT;
			if (opngen.calcremain > opncfg.calc1024) {
				samp_l += opngen.outdl * opncfg.calc1024;
				samp_r += opngen.outdr * opncfg.calc1024;
				opngen.calcremain -= opncfg.calc1024;
			}
			else {
				break;
			}
		}
		samp_l += opngen.outdl * opngen.calcremain;
		samp_l >>= 8;
		samp_l *= opncfg.fmvol;
		samp_l >>= (OPM_OUTSB + FMDIV_BITS + 1 + 6 - FMVOL_SFTBIT - 8);
		pcm[0] += samp_l;
		samp_r += opngen.outdr * opngen.calcremain;
		samp_r >>= 8;
		samp_r *= opncfg.fmvol;
		samp_r >>= (OPM_OUTSB + FMDIV_BITS + 1 + 6 - FMVOL_SFTBIT - 8);
		pcm[1] += samp_r;
		opngen.calcremain -= opncfg.calc1024;
		pcm += 2;
	} while(--count);
	opngen.playing = playing;
	(void)hdl;
}

void SOUNDCALL opngen_getpcmvr(void *hdl, SINT32 *pcm, UINT count) {

	OPNCH	*fm;
	UINT	i;
	SINT32	samp_l;
	SINT32	samp_r;

	fm = opnch;
	while(count--) {
		samp_l = opngen.outdl * (opngen.calcremain * -1);
		samp_r = opngen.outdr * (opngen.calcremain * -1);
		opngen.calcremain += FMDIV_ENT;
		while(1) {
			opngen.outdc = 0;
			opngen.outdl = 0;
			opngen.outdr = 0;
			for (i=0; i<opngen.playchannels; i++) {
				calcratechannel(fm + i);
			}
			if (opncfg.vr_en) {
				SINT32 tmpl;
				SINT32 tmpr;
				tmpl = (opngen.outdl >> 5) * opncfg.vr_l;
				tmpr = (opngen.outdr >> 5) * opncfg.vr_r;
				opngen.outdl += tmpr;
				opngen.outdr += tmpl;
			}
			opngen.outdl += opngen.outdc;
			opngen.outdr += opngen.outdc;
			opngen.outdl >>= FMVOL_SFTBIT;
			opngen.outdr >>= FMVOL_SFTBIT;
			if (opngen.calcremain > opncfg.calc1024) {
				samp_l += opngen.outdl * opncfg.calc1024;
				samp_r += opngen.outdr * opncfg.calc1024;
				opngen.calcremain -= opncfg.calc1024;
			}
			else {
				break;
			}
		}
		samp_l += opngen.outdl * opngen.calcremain;
		samp_l >>= 8;
		samp_l *= opncfg.fmvol;
		samp_l >>= (OPM_OUTSB + FMDIV_BITS + 1 + 6 - FMVOL_SFTBIT - 8);
		pcm[0] += samp_l;
		samp_r += opngen.outdr * opngen.calcremain;
		samp_r >>= 8;
		samp_r *= opncfg.fmvol;
		samp_r >>= (OPM_OUTSB + FMDIV_BITS + 1 + 6 - FMVOL_SFTBIT - 8);
		pcm[1] += samp_r;
		opngen.calcremain -= opncfg.calc1024;
		pcm += 2;
	}
	(void)hdl;
}

