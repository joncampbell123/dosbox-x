// license:GPL-2.0+
// copyright-holders:Jarek Burczynski,Tatsuyuki Satoh
/*
**
** File: fmopl.c - software implementation of FM sound generator
**                                            types OPL and OPL2
**
** Copyright Jarek Burczynski (bujar at mame dot net)
** Copyright Tatsuyuki Satoh , MultiArcadeMachineEmulator development
**
** Version 0.72
**

Revision History:

04-08-2003 Jarek Burczynski:
 - removed BFRDY hack. BFRDY is busy flag, and it should be 0 only when the chip
   handles memory read/write or during the adpcm synthesis when the chip
   requests another byte of ADPCM data.

24-07-2003 Jarek Burczynski:
 - added a small hack for Y8950 status BFRDY flag (bit 3 should be set after
   some (unknown) delay). Right now it's always set.

14-06-2003 Jarek Burczynski:
 - implemented all of the status register flags in Y8950 emulation
 - renamed y8950_set_delta_t_memory() parameters from _rom_ to _mem_ since
   they can be either RAM or ROM

08-10-2002 Jarek Burczynski (thanks to Dox for the YM3526 chip)
 - corrected ym3526_read() to always set bit 2 and bit 1
   to HIGH state - identical to ym3812_read (verified on real YM3526)

04-28-2002 Jarek Burczynski:
 - binary exact Envelope Generator (verified on real YM3812);
   compared to YM2151: the EG clock is equal to internal_clock,
   rates are 2 times slower and volume resolution is one bit less
 - modified interface functions (they no longer return pointer -
   that's internal to the emulator now):
    - new wrapper functions for OPLCreate: ym3526_init(), ym3812_init() and y8950_init()
 - corrected 'off by one' error in feedback calculations (when feedback is off)
 - enabled waveform usage (credit goes to Vlad Romascanu and zazzal22)
 - speeded up noise generator calculations (Nicola Salmoria)

03-24-2002 Jarek Burczynski (thanks to Dox for the YM3812 chip)
 Complete rewrite (all verified on real YM3812):
 - corrected sin_tab and tl_tab data
 - corrected operator output calculations
 - corrected waveform_select_enable register;
   simply: ignore all writes to waveform_select register when
   waveform_select_enable == 0 and do not change the waveform previously selected.
 - corrected KSR handling
 - corrected Envelope Generator: attack shape, Sustain mode and
   Percussive/Non-percussive modes handling
 - Envelope Generator rates are two times slower now
 - LFO amplitude (tremolo) and phase modulation (vibrato)
 - rhythm sounds phase generation
 - white noise generator (big thanks to Olivier Galibert for mentioning Berlekamp-Massey algorithm)
 - corrected key on/off handling (the 'key' signal is ORed from three sources: FM, rhythm and CSM)
 - funky details (like ignoring output of operator 1 in BD rhythm sound when connect == 1)

12-28-2001 Acho A. Tang
 - reflected Delta-T EOS status on Y8950 status port.
 - fixed subscription range of attack/decay tables


    To do:
        add delay before key off in CSM mode (see CSMKeyControll)
        verify volume of the FM part on the Y8950
*/

#include "emu.h"
#include "ymdeltat.h"
#include "fmopl.h"



/* output final shift */
#if (OPL_SAMPLE_BITS==16)
	#define FINAL_SH    (0)
	#define MAXOUT      (+32767)
	#define MINOUT      (-32768)
#else
	#define FINAL_SH    (8)
	#define MAXOUT      (+127)
	#define MINOUT      (-128)
#endif


#define FREQ_SH         16  /* 16.16 fixed point (frequency calculations) */
#define EG_SH           16  /* 16.16 fixed point (EG timing)              */
#define LFO_SH          24  /*  8.24 fixed point (LFO calculations)       */
#define TIMER_SH        16  /* 16.16 fixed point (timers calculations)    */

#define FREQ_MASK       ((1<<FREQ_SH)-1)

/* envelope output entries */
#define ENV_BITS        10
#define ENV_LEN         (1<<ENV_BITS)
#define ENV_STEP        (128.0/ENV_LEN)

#define MAX_ATT_INDEX   ((1<<(ENV_BITS-1))-1) /*511*/
#define MIN_ATT_INDEX   (0)

/* sinwave entries */
#define SIN_BITS        10
#define SIN_LEN         (1<<SIN_BITS)
#define SIN_MASK        (SIN_LEN-1)
#define DV				(0.1875 / 2.0 )

#define TL_RES_LEN      (256)   /* 8 bits addressing (real chip) */
#define TL_TAB_LEN		(12 * 2 * TL_RES_LEN)

/*  TL_TAB_LEN is calculated as:
*   12 - sinus amplitude bits     (Y axis)
*   2  - sinus sign bit           (Y axis)
*   TL_RES_LEN - sinus resolution (X axis)
*/


#define ENV_QUIET      ( TL_TAB_LEN >> 4 )
#define LFO_AM_TAB_ELEMENTS 210



/* register number to channel number , slot offset */
#define SLOT1 0
#define SLOT2 1

/* Envelope Generator phases */

#define EG_ATT          4
#define EG_DEC          3
#define EG_SUS          2
#define EG_REL          1
#define EG_OFF          0


/* save output as raw 16-bit sample */

/*#define SAVE_SAMPLE*/

#ifdef SAVE_SAMPLE
static inline signed int acc_calc(signed int value)
{
	if (value>=0)
	{
		if (value < 0x0200)
			return (value & ~0);
		if (value < 0x0400)
			return (value & ~1);
		if (value < 0x0800)
			return (value & ~3);
		if (value < 0x1000)
			return (value & ~7);
		if (value < 0x2000)
			return (value & ~15);
		if (value < 0x4000)
			return (value & ~31);
		return (value & ~63);
	}
	/*else value < 0*/
	if (value > -0x0200)
		return (~abs(value) & ~0);
	if (value > -0x0400)
		return (~abs(value) & ~1);
	if (value > -0x0800)
		return (~abs(value) & ~3);
	if (value > -0x1000)
		return (~abs(value) & ~7);
	if (value > -0x2000)
		return (~abs(value) & ~15);
	if (value > -0x4000)
		return (~abs(value) & ~31);
	return (~abs(value) & ~63);
}


static FILE *sample[1];
	#if 1   /*save to MONO file */
		#define SAVE_ALL_CHANNELS \
		{   signed int pom = acc_calc(lt); \
			fputc((unsigned short)pom&0xff,sample[0]); \
			fputc(((unsigned short)pom>>8)&0xff,sample[0]); \
		}
	#else   /*save to STEREO file */
		#define SAVE_ALL_CHANNELS \
		{   signed int pom = lt; \
			fputc((unsigned short)pom&0xff,sample[0]); \
			fputc(((unsigned short)pom>>8)&0xff,sample[0]); \
			pom = rt; \
			fputc((unsigned short)pom&0xff,sample[0]); \
			fputc(((unsigned short)pom>>8)&0xff,sample[0]); \
		}
	#endif
#endif

#define OPL_TYPE_WAVESEL   0x01  /* waveform select     */
#define OPL_TYPE_ADPCM     0x02  /* DELTA-T ADPCM unit  */
#define OPL_TYPE_KEYBOARD  0x04  /* keyboard interface  */
#define OPL_TYPE_IO        0x08  /* I/O port            */

/* ---------- Generic interface section ---------- */
#define OPL_TYPE_YM3526 (0)
#define OPL_TYPE_YM3812 (OPL_TYPE_WAVESEL)
#define OPL_TYPE_Y8950  (OPL_TYPE_ADPCM|OPL_TYPE_KEYBOARD|OPL_TYPE_IO)


namespace {

// TODO: make these static members

#define RATE_STEPS (8)
extern const unsigned char eg_rate_shift[16+64+16];
extern const unsigned char eg_rate_select[16+64+16];


struct OPL_SLOT
{
	uint32_t  ar;         /* attack rate: AR<<2           */
	uint32_t  dr;         /* decay rate:  DR<<2           */
	uint32_t  rr;         /* release rate:RR<<2           */
	uint8_t   KSR;        /* key scale rate               */
	uint8_t   ksl;        /* keyscale level               */
	uint8_t   ksr;        /* key scale rate: kcode>>KSR   */
	uint8_t   mul;        /* multiple: mul_tab[ML]        */

	/* Phase Generator */
	uint32_t  Cnt;        /* frequency counter            */
	uint32_t  Incr;       /* frequency counter step       */
	uint8_t   FB;         /* feedback shift value         */
	int32_t   *connect1;  /* slot1 output pointer         */
	int32_t   op1_out[2]; /* slot1 output for feedback    */
	uint8_t   CON;        /* connection (algorithm) type  */

	/* Envelope Generator */
	uint8_t   eg_type;    /* percussive/non-percussive mode */
	uint8_t   state;      /* phase type                   */
	uint32_t  TL;         /* total level: TL << 2         */
	int32_t   TLL;        /* adjusted now TL              */
	int32_t   volume;     /* envelope counter             */
	int32_t   sl;         /* sustain level: sl_tab[SL]    */
	uint8_t   eg_sh_ar;   /* (attack state)               */
	uint8_t   eg_sel_ar;  /* (attack state)               */
	uint8_t   eg_sh_dr;   /* (decay state)                */
	uint8_t   eg_sel_dr;  /* (decay state)                */
	uint8_t   eg_sh_rr;   /* (release state)              */
	uint8_t   eg_sel_rr;  /* (release state)              */
	uint32_t  key;        /* 0 = KEY OFF, >0 = KEY ON     */

	/* LFO */
	uint32_t  AMmask;     /* LFO Amplitude Modulation enable mask */
	uint8_t   vib;        /* LFO Phase Modulation enable flag (active high)*/

	/* waveform select */
	uint16_t  wavetable;

	void KEYON(uint32_t key_set)
	{
		if( !key )
		{
			/* restart Phase Generator */
			Cnt = 0;
			/* phase -> Attack */
			state = EG_ATT;
		}
		key |= key_set;
	}

	void KEYOFF(uint32_t key_clr)
	{
		if( key )
		{
			key &= key_clr;

			if( !key )
			{
				/* phase -> Release */
				if (state>EG_REL)
					state = EG_REL;
			}
		}
	}
};

struct OPL_CH
{
	OPL_SLOT SLOT[2];
	/* phase generator state */
	uint32_t  block_fnum; /* block+fnum                   */
	uint32_t  fc;         /* Freq. Increment base         */
	uint32_t  ksl_base;   /* KeyScaleLevel Base step      */
	uint8_t   kcode;      /* key code (for key scaling)   */


	/* update phase increment counter of operator (also update the EG rates if necessary) */
	void CALC_FCSLOT(OPL_SLOT &SLOT)
	{
		/* (frequency) phase increment counter */
		SLOT.Incr = fc * SLOT.mul;
		int const ksr = kcode >> SLOT.KSR;

		if( SLOT.ksr != ksr )
		{
			SLOT.ksr = ksr;

			/* calculate envelope generator rates */
			if ((SLOT.ar + SLOT.ksr) < 16+62)
			{
				SLOT.eg_sh_ar  = eg_rate_shift [SLOT.ar + SLOT.ksr ];
				SLOT.eg_sel_ar = eg_rate_select[SLOT.ar + SLOT.ksr ];
			}
			else
			{
				SLOT.eg_sh_ar  = 0;
				SLOT.eg_sel_ar = 13*RATE_STEPS;
			}
			SLOT.eg_sh_dr  = eg_rate_shift [SLOT.dr + SLOT.ksr ];
			SLOT.eg_sel_dr = eg_rate_select[SLOT.dr + SLOT.ksr ];
			SLOT.eg_sh_rr  = eg_rate_shift [SLOT.rr + SLOT.ksr ];
			SLOT.eg_sel_rr = eg_rate_select[SLOT.rr + SLOT.ksr ];
		}
	}
};

/* OPL state */
struct FM_OPL
{
	/* FM channel slots */
	OPL_CH  P_CH[9];                /* OPL/OPL2 chips have 9 channels*/

	uint32_t  eg_cnt;                 /* global envelope generator counter    */
	uint32_t  eg_timer;               /* global envelope generator counter works at frequency = chipclock/72 */
	uint32_t  eg_timer_add;           /* step of eg_timer                     */
	uint32_t  eg_timer_overflow;      /* envelope generator timer overflows every 1 sample (on real chip) */

	uint8_t   rhythm;                 /* Rhythm mode                  */

	uint32_t  fn_tab[1024];           /* fnumber->increment counter   */

	/* LFO */
	uint32_t  LFO_AM;
	int32_t   LFO_PM;

	uint8_t   lfo_am_depth;
	uint8_t   lfo_pm_depth_range;
	uint32_t  lfo_am_cnt;
	uint32_t  lfo_am_inc;
	uint32_t  lfo_pm_cnt;
	uint32_t  lfo_pm_inc;

	uint32_t  noise_rng;              /* 23 bit noise shift register  */
	uint32_t  noise_p;                /* current noise 'phase'        */
	uint32_t  noise_f;                /* current noise period         */

	uint8_t   wavesel;                /* waveform select enable flag  */

	uint32_t  T[2];                   /* timer counters               */
	uint8_t   st[2];                  /* timer enable                 */

#if BUILD_Y8950
	/* Delta-T ADPCM unit (Y8950) */

	YM_DELTAT *deltat;

	/* Keyboard and I/O ports interface */
	uint8_t   portDirection;
	uint8_t   portLatch;
	OPL_PORTHANDLER_R porthandler_r;
	OPL_PORTHANDLER_W porthandler_w;
	device_t *  port_param;
	OPL_PORTHANDLER_R keyboardhandler_r;
	OPL_PORTHANDLER_W keyboardhandler_w;
	device_t *  keyboard_param;
#endif

	/* external event callback handlers */
	OPL_TIMERHANDLER  timer_handler;    /* TIMER handler                */
	device_t *TimerParam;                   /* TIMER parameter              */
	OPL_IRQHANDLER    IRQHandler;   /* IRQ handler                  */
	device_t *IRQParam;                 /* IRQ parameter                */
	OPL_UPDATEHANDLER UpdateHandler;/* stream update handler        */
	device_t *UpdateParam;              /* stream update parameter      */

	uint8_t type;                     /* chip type                    */
	uint8_t address;                  /* address register             */
	uint8_t status;                   /* status flag                  */
	uint8_t statusmask;               /* status mask                  */
	uint8_t mode;                     /* Reg.08 : CSM,notesel,etc.    */

	uint32_t clock;                   /* master clock  (Hz)           */
	uint32_t rate;                    /* sampling rate (Hz)           */
	double freqbase;                /* frequency base               */
	//attotime TimerBase;         /* Timer base time (==sampling time)*/
	device_t *device;

	signed int phase_modulation;    /* phase modulation input (SLOT 2) */
	signed int output[1];
#if BUILD_Y8950
	int32_t output_deltat[4];     /* for Y8950 DELTA-T, chip is mono, that 4 here is just for safety */
#endif


	/* status set and IRQ handling */
	void STATUS_SET(int flag)
	{
		/* set status flag */
		status |= flag;
		if(!(status & 0x80))
		{
			if(status & statusmask)
			{   /* IRQ on */
				status |= 0x80;
				/* callback user interrupt handler (IRQ is OFF to ON) */
				if(IRQHandler) (IRQHandler)(IRQParam,1);
			}
		}
	}

	/* status reset and IRQ handling */
	void STATUS_RESET(int flag)
	{
		/* reset status flag */
		status &=~flag;
		if(status & 0x80)
		{
			if (!(status & statusmask) )
			{
				status &= 0x7f;
				/* callback user interrupt handler (IRQ is ON to OFF) */
				if(IRQHandler) (IRQHandler)(IRQParam,0);
			}
		}
	}

	/* IRQ mask set */
	void STATUSMASK_SET(int flag)
	{
		statusmask = flag;
		/* IRQ handling check */
		STATUS_SET(0);
		STATUS_RESET(0);
	}


	/* advance LFO to next sample */
	void advance_lfo()
	{
		/* LFO */
		lfo_am_cnt += lfo_am_inc;
		if (lfo_am_cnt >= (uint32_t(LFO_AM_TAB_ELEMENTS) << LFO_SH))  /* lfo_am_table is 210 elements long */
			lfo_am_cnt -= (uint32_t(LFO_AM_TAB_ELEMENTS) << LFO_SH);

		uint8_t const tmp = lfo_am_table[ lfo_am_cnt >> LFO_SH ];

		LFO_AM = lfo_am_depth ? tmp : tmp >> 2;

		lfo_pm_cnt += lfo_pm_inc;
		LFO_PM = (lfo_pm_cnt>>LFO_SH & 7) | lfo_pm_depth_range;
	}

	/* advance to next sample */
	void advance()
	{
		eg_timer += eg_timer_add;

		while (eg_timer >= eg_timer_overflow)
		{
			eg_timer -= eg_timer_overflow;

			eg_cnt++;

			for (int i=0; i<9*2; i++)
			{
				OPL_CH   &CH  = P_CH[i/2];
				OPL_SLOT &op  = CH.SLOT[i&1];

				/* Envelope Generator */
				switch(op.state)
				{
				case EG_ATT:        /* attack phase */
					if ( !(eg_cnt & ((1<<op.eg_sh_ar)-1) ) )
					{
						op.volume += (~op.volume *
													(eg_inc[op.eg_sel_ar + ((eg_cnt>>op.eg_sh_ar)&7)])
													) >>3;

						if (op.volume <= MIN_ATT_INDEX)
						{
							op.volume = MIN_ATT_INDEX;
							op.state = EG_DEC;
						}

					}
					break;

				case EG_DEC:    /* decay phase */
					if ( !(eg_cnt & ((1<<op.eg_sh_dr)-1) ) )
					{
						op.volume += eg_inc[op.eg_sel_dr + ((eg_cnt>>op.eg_sh_dr)&7)];

						if ( op.volume >= op.sl )
							op.state = EG_SUS;

					}
					break;

				case EG_SUS:    /* sustain phase */

					/* this is important behaviour:
					one can change percussive/non-percussive modes on the fly and
					the chip will remain in sustain phase - verified on real YM3812 */

					if(op.eg_type)     /* non-percussive mode */
					{
										/* do nothing */
					}
					else                /* percussive mode */
					{
						/* during sustain phase chip adds Release Rate (in percussive mode) */
						if ( !(eg_cnt & ((1<<op.eg_sh_rr)-1) ) )
						{
							op.volume += eg_inc[op.eg_sel_rr + ((eg_cnt>>op.eg_sh_rr)&7)];

							if ( op.volume >= MAX_ATT_INDEX )
								op.volume = MAX_ATT_INDEX;
						}
						/* else do nothing in sustain phase */
					}
					break;

				case EG_REL:    /* release phase */
					if ( !(eg_cnt & ((1<<op.eg_sh_rr)-1) ) )
					{
						op.volume += eg_inc[op.eg_sel_rr + ((eg_cnt>>op.eg_sh_rr)&7)];

						if ( op.volume >= MAX_ATT_INDEX )
						{
							op.volume = MAX_ATT_INDEX;
							op.state = EG_OFF;
						}

					}
					break;

				default:
					break;
				}
			}
		}

		for (int i=0; i<9*2; i++)
		{
			OPL_CH   &CH  = P_CH[i/2];
			OPL_SLOT &op  = CH.SLOT[i&1];

			/* Phase Generator */
			if(op.vib)
			{
				unsigned int block_fnum                    = CH.block_fnum;
				unsigned int const fnum_lfo                = (block_fnum&0x0380) >> 7;

				signed int const lfo_fn_table_index_offset = lfo_pm_table[LFO_PM + 16*fnum_lfo ];

				if (lfo_fn_table_index_offset)  /* LFO phase modulation active */
				{
					block_fnum += lfo_fn_table_index_offset;
					uint8_t const block = (block_fnum&0x1c00) >> 10;
					op.Cnt += (fn_tab[block_fnum&0x03ff] >> (7-block)) * op.mul;
				}
				else    /* LFO phase modulation  = zero */
				{
					op.Cnt += op.Incr;
				}
			}
			else    /* LFO phase modulation disabled for this operator */
			{
				op.Cnt += op.Incr;
			}
		}

		/*  The Noise Generator of the YM3812 is 23-bit shift register.
		*   Period is equal to 2^23-2 samples.
		*   Register works at sampling frequency of the chip, so output
		*   can change on every sample.
		*
		*   Output of the register and input to the bit 22 is:
		*   bit0 XOR bit14 XOR bit15 XOR bit22
		*
		*   Simply use bit 22 as the noise output.
		*/

		noise_p += noise_f;
		int i = noise_p >> FREQ_SH;        /* number of events (shifts of the shift register) */
		noise_p &= FREQ_MASK;
		while (i)
		{
			/*
			uint32_t j;
			j = ( (noise_rng) ^ (noise_rng>>14) ^ (noise_rng>>15) ^ (noise_rng>>22) ) & 1;
			noise_rng = (j<<22) | (noise_rng>>1);
			*/

			/*
			    Instead of doing all the logic operations above, we
			    use a trick here (and use bit 0 as the noise output).
			    The difference is only that the noise bit changes one
			    step ahead. This doesn't matter since we don't know
			    what is real state of the noise_rng after the reset.
			*/

			if (noise_rng & 1) noise_rng ^= 0x800302;
			noise_rng >>= 1;

			i--;
		}
	}

	/* calculate output */
	void CALC_CH(OPL_CH &CH)
	{
		OPL_SLOT *SLOT;
		unsigned int env;
		signed int out;

		phase_modulation = 0;

		/* SLOT 1 */
		SLOT = &CH.SLOT[SLOT1];
		env  = volume_calc(*SLOT);
		out  = SLOT->op1_out[0] + SLOT->op1_out[1];
		SLOT->op1_out[0] = SLOT->op1_out[1];
		*SLOT->connect1 += SLOT->op1_out[0];
		SLOT->op1_out[1] = 0;
		if( env < ENV_QUIET )
		{
			if (!SLOT->FB)
				out = 0;
			SLOT->op1_out[1] = op_calc1(SLOT->Cnt, env, (out<<SLOT->FB), SLOT->wavetable );
		}

		/* SLOT 2 */
		SLOT++;
		env = volume_calc(*SLOT);
		if( env < ENV_QUIET )
			output[0] += op_calc(SLOT->Cnt, env, phase_modulation, SLOT->wavetable);
	}

	/*
	    operators used in the rhythm sounds generation process:

	    Envelope Generator:

	channel  operator  register number   Bass  High  Snare Tom  Top
	/ slot   number    TL ARDR SLRR Wave Drum  Hat   Drum  Tom  Cymbal
	 6 / 0   12        50  70   90   f0  +
	 6 / 1   15        53  73   93   f3  +
	 7 / 0   13        51  71   91   f1        +
	 7 / 1   16        54  74   94   f4              +
	 8 / 0   14        52  72   92   f2                    +
	 8 / 1   17        55  75   95   f5                          +

	    Phase Generator:

	channel  operator  register number   Bass  High  Snare Tom  Top
	/ slot   number    MULTIPLE          Drum  Hat   Drum  Tom  Cymbal
	 6 / 0   12        30                +
	 6 / 1   15        33                +
	 7 / 0   13        31                      +     +           +
	 7 / 1   16        34                -----  n o t  u s e d -----
	 8 / 0   14        32                                  +
	 8 / 1   17        35                      +                 +

	channel  operator  register number   Bass  High  Snare Tom  Top
	number   number    BLK/FNUM2 FNUM    Drum  Hat   Drum  Tom  Cymbal
	   6     12,15     B6        A6      +

	   7     13,16     B7        A7            +     +           +

	   8     14,17     B8        A8            +           +     +

	*/

	/* calculate rhythm */

	void CALC_RH()
	{
		unsigned int const noise = BIT(noise_rng, 0);

		OPL_SLOT *SLOT;
		signed int out;
		unsigned int env;


		/* Bass Drum (verified on real YM3812):
		  - depends on the channel 6 'connect' register:
		      when connect = 0 it works the same as in normal (non-rhythm) mode (op1->op2->out)
		      when connect = 1 _only_ operator 2 is present on output (op2->out), operator 1 is ignored
		  - output sample always is multiplied by 2
		*/

		phase_modulation = 0;
		/* SLOT 1 */
		SLOT = &P_CH[6].SLOT[SLOT1];
		env = volume_calc(*SLOT);

		out = SLOT->op1_out[0] + SLOT->op1_out[1];
		SLOT->op1_out[0] = SLOT->op1_out[1];

		if (!SLOT->CON)
			phase_modulation = SLOT->op1_out[0];
		/* else ignore output of operator 1 */

		SLOT->op1_out[1] = 0;
		if( env < ENV_QUIET )
		{
			if (!SLOT->FB)
				out = 0;
			SLOT->op1_out[1] = op_calc1(SLOT->Cnt, env, (out<<SLOT->FB), SLOT->wavetable );
		}

		/* SLOT 2 */
		SLOT++;
		env = volume_calc(*SLOT);
		if( env < ENV_QUIET )
			output[0] += op_calc(SLOT->Cnt, env, phase_modulation, SLOT->wavetable) * 2;


		/* Phase generation is based on: */
		/* HH  (13) channel 7->slot 1 combined with channel 8->slot 2 (same combination as TOP CYMBAL but different output phases) */
		/* SD  (16) channel 7->slot 1 */
		/* TOM (14) channel 8->slot 1 */
		/* TOP (17) channel 7->slot 1 combined with channel 8->slot 2 (same combination as HIGH HAT but different output phases) */

		/* Envelope generation based on: */
		/* HH  channel 7->slot1 */
		/* SD  channel 7->slot2 */
		/* TOM channel 8->slot1 */
		/* TOP channel 8->slot2 */


		/* The following formulas can be well optimized.
		   I leave them in direct form for now (in case I've missed something).
		*/

		/* High Hat (verified on real YM3812) */
		OPL_SLOT const &SLOT7_1 = P_CH[7].SLOT[SLOT1];
		OPL_SLOT const &SLOT8_2 = P_CH[8].SLOT[SLOT2];
		env = volume_calc(SLOT7_1);
		if( env < ENV_QUIET )
		{
			/* high hat phase generation:
			    phase = d0 or 234 (based on frequency only)
			    phase = 34 or 2d0 (based on noise)
			*/

			/* base frequency derived from operator 1 in channel 7 */
			unsigned char const bit7 = BIT(SLOT7_1.Cnt >> FREQ_SH, 7);
			unsigned char const bit3 = BIT(SLOT7_1.Cnt >> FREQ_SH, 3);
			unsigned char const bit2 = BIT(SLOT7_1.Cnt >> FREQ_SH, 2);

			unsigned char const res1 = (bit2 ^ bit7) | bit3;

			/* when res1 = 0 phase = 0x000 | 0xd0; */
			/* when res1 = 1 phase = 0x200 | (0xd0>>2); */
			uint32_t phase = res1 ? (0x200|(0xd0>>2)) : 0xd0;

			/* enable gate based on frequency of operator 2 in channel 8 */
			unsigned char const bit5e= BIT(SLOT8_2.Cnt >> FREQ_SH, 5);
			unsigned char const bit3e= BIT(SLOT8_2.Cnt >> FREQ_SH, 3);

			unsigned char const res2 = bit3e ^ bit5e;

			/* when res2 = 0 pass the phase from calculation above (res1); */
			/* when res2 = 1 phase = 0x200 | (0xd0>>2); */
			if (res2)
				phase = (0x200|(0xd0>>2));


			/* when phase & 0x200 is set and noise=1 then phase = 0x200|0xd0 */
			/* when phase & 0x200 is set and noise=0 then phase = 0x200|(0xd0>>2), ie no change */
			if (phase&0x200)
			{
				if (noise)
					phase = 0x200|0xd0;
			}
			else
			/* when phase & 0x200 is clear and noise=1 then phase = 0xd0>>2 */
			/* when phase & 0x200 is clear and noise=0 then phase = 0xd0, ie no change */
			{
				if (noise)
					phase = 0xd0>>2;
			}

			output[0] += op_calc(phase<<FREQ_SH, env, 0, SLOT7_1.wavetable) * 2;
		}

		/* Snare Drum (verified on real YM3812) */
		OPL_SLOT const &SLOT7_2 = P_CH[7].SLOT[SLOT2];
		env = volume_calc(SLOT7_2);
		if( env < ENV_QUIET )
		{
			/* base frequency derived from operator 1 in channel 7 */
			unsigned char const bit8 = BIT(SLOT7_1.Cnt >> FREQ_SH, 8);

			/* when bit8 = 0 phase = 0x100; */
			/* when bit8 = 1 phase = 0x200; */
			uint32_t phase = bit8 ? 0x200 : 0x100;

			/* Noise bit XOR'es phase by 0x100 */
			/* when noisebit = 0 pass the phase from calculation above */
			/* when noisebit = 1 phase ^= 0x100; */
			/* in other words: phase ^= (noisebit<<8); */
			if (noise)
				phase ^= 0x100;

			output[0] += op_calc(phase<<FREQ_SH, env, 0, SLOT7_2.wavetable) * 2;
		}

		/* Tom Tom (verified on real YM3812) */
		OPL_SLOT const &SLOT8_1 = P_CH[8].SLOT[SLOT1];
		env = volume_calc(SLOT8_1);
		if( env < ENV_QUIET )
			output[0] += op_calc(SLOT8_1.Cnt, env, 0, SLOT8_1.wavetable) * 2;

		/* Top Cymbal (verified on real YM3812) */
		env = volume_calc(SLOT8_2);
		if( env < ENV_QUIET )
		{
			/* base frequency derived from operator 1 in channel 7 */
			unsigned char const bit7 = BIT(SLOT7_1.Cnt >> FREQ_SH, 7);
			unsigned char const bit3 = BIT(SLOT7_1.Cnt >> FREQ_SH, 3);
			unsigned char const bit2 = BIT(SLOT7_1.Cnt >> FREQ_SH, 2);

			unsigned char const res1 = (bit2 ^ bit7) | bit3;

			/* when res1 = 0 phase = 0x000 | 0x100; */
			/* when res1 = 1 phase = 0x200 | 0x100; */
			uint32_t phase = res1 ? 0x300 : 0x100;

			/* enable gate based on frequency of operator 2 in channel 8 */
			unsigned char const bit5e= BIT(SLOT8_2.Cnt >> FREQ_SH, 5);
			unsigned char const bit3e= BIT(SLOT8_2.Cnt >> FREQ_SH, 3);

			unsigned char const res2 = bit3e ^ bit5e;
			/* when res2 = 0 pass the phase from calculation above (res1); */
			/* when res2 = 1 phase = 0x200 | 0x100; */
			if (res2)
				phase = 0x300;

			output[0] += op_calc(phase<<FREQ_SH, env, 0, SLOT8_2.wavetable) * 2;
		}
	}


	void initialize();


	/* set multi,am,vib,EG-TYP,KSR,mul */
	void set_mul(int slot, int v)
	{
		OPL_CH   &CH   = P_CH[slot/2];
		OPL_SLOT &SLOT = CH.SLOT[slot&1];

		SLOT.mul     = mul_tab[v&0x0f];
		SLOT.KSR     = (v & 0x10) ? 0 : 2;
		SLOT.eg_type = (v & 0x20);
		SLOT.vib     = (v & 0x40);
		SLOT.AMmask  = (v & 0x80) ? ~0 : 0;
		CH.CALC_FCSLOT(SLOT);
	}

	/* set ksl & tl */
	void set_ksl_tl(int slot, int v)
	{
		OPL_CH   &CH   = P_CH[slot/2];
		OPL_SLOT &SLOT = CH.SLOT[slot&1];

		SLOT.ksl = ksl_shift[v >> 6];
		SLOT.TL  = (v&0x3f)<<(ENV_BITS-1-7); /* 7 bits TL (bit 6 = always 0) */

		SLOT.TLL = SLOT.TL + (CH.ksl_base >> SLOT.ksl);
	}

	/* set attack rate & decay rate  */
	void set_ar_dr(int slot, int v)
	{
		OPL_CH   &CH   = P_CH[slot/2];
		OPL_SLOT &SLOT = CH.SLOT[slot&1];

		SLOT.ar = (v>>4)  ? 16 + ((v>>4)  <<2) : 0;

		if ((SLOT.ar + SLOT.ksr) < 16+62)
		{
			SLOT.eg_sh_ar  = eg_rate_shift [SLOT.ar + SLOT.ksr ];
			SLOT.eg_sel_ar = eg_rate_select[SLOT.ar + SLOT.ksr ];
		}
		else
		{
			SLOT.eg_sh_ar  = 0;
			SLOT.eg_sel_ar = 13*RATE_STEPS;
		}

		SLOT.dr    = (v&0x0f)? 16 + ((v&0x0f)<<2) : 0;
		SLOT.eg_sh_dr  = eg_rate_shift [SLOT.dr + SLOT.ksr ];
		SLOT.eg_sel_dr = eg_rate_select[SLOT.dr + SLOT.ksr ];
	}

	/* set sustain level & release rate */
	void set_sl_rr(int slot, int v)
	{
		OPL_CH   &CH   = P_CH[slot/2];
		OPL_SLOT &SLOT = CH.SLOT[slot&1];

		SLOT.sl  = sl_tab[ v>>4 ];

		SLOT.rr  = (v&0x0f)? 16 + ((v&0x0f)<<2) : 0;
		SLOT.eg_sh_rr  = eg_rate_shift [SLOT.rr + SLOT.ksr ];
		SLOT.eg_sel_rr = eg_rate_select[SLOT.rr + SLOT.ksr ];
	}


	void WriteReg(int r, int v);
	void ResetChip();
	//void postload();


	/* lock/unlock for common table */
	static int LockTable(device_t *device)
	{
        (void)device;
		num_lock++;
		if(num_lock>1) return 0;

		/* first time */

		/* allocate total level table (128kb space) */
		if( !init_tables() )
		{
			num_lock--;
			return -1;
		}

		return 0;
	}

	static void UnLockTable()
	{
		if(num_lock) num_lock--;
		if(num_lock) return;

		/* last time */
		CloseTable();
	}

private:
	uint32_t volume_calc(OPL_SLOT const &OP) const
	{
		return OP.TLL + uint32_t(OP.volume) + (LFO_AM & OP.AMmask);
	}

	static inline signed int op_calc(uint32_t phase, unsigned int env, signed int pm, unsigned int wave_tab)
	{
		uint32_t const p = (env<<4) + sin_tab[wave_tab + ((((signed int)((phase & ~FREQ_MASK) + (pm<<16))) >> FREQ_SH ) & SIN_MASK) ];

		return (p >= TL_TAB_LEN) ? 0 : tl_tab[p];
	}

	static inline signed int op_calc1(uint32_t phase, unsigned int env, signed int pm, unsigned int wave_tab)
	{
		uint32_t const p = (env<<4) + sin_tab[wave_tab + ((((signed int)((phase & ~FREQ_MASK) + pm      )) >> FREQ_SH ) & SIN_MASK) ];

		return (p >= TL_TAB_LEN) ? 0 : tl_tab[p];
	}


	static int init_tables();

	static void CloseTable()
	{
#ifdef SAVE_SAMPLE
		fclose(sample[0]);
#endif
	}

	static const double ksl_tab[8*16];
	static const uint32_t ksl_shift[4];
	static const int32_t sl_tab[16];
	static const unsigned char eg_inc[15 * RATE_STEPS];

	static const uint8_t mul_tab[16];
	static signed int tl_tab[TL_TAB_LEN];
	static unsigned int sin_tab[SIN_LEN * 4];

	static const uint8_t lfo_am_table[LFO_AM_TAB_ELEMENTS];
	static const int8_t lfo_pm_table[8 * 8 * 2];

	static int num_lock;
};



/* mapping of register number (offset) to slot number used by the emulator */
static const int slot_array[32]=
{
		0, 2, 4, 1, 3, 5,-1,-1,
		6, 8,10, 7, 9,11,-1,-1,
	12,14,16,13,15,17,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1
};

/* key scale level */
/* table is 3dB/octave , DV converts this into 6dB/octave */
/* 0.1875 is bit 0 weight of the envelope counter (volume) expressed in the 'decibel' scale */
const double FM_OPL::ksl_tab[8*16]=
{
	/* OCT 0 */
		0.000/DV, 0.000/DV, 0.000/DV, 0.000/DV,
		0.000/DV, 0.000/DV, 0.000/DV, 0.000/DV,
		0.000/DV, 0.000/DV, 0.000/DV, 0.000/DV,
		0.000/DV, 0.000/DV, 0.000/DV, 0.000/DV,
	/* OCT 1 */
		0.000/DV, 0.000/DV, 0.000/DV, 0.000/DV,
		0.000/DV, 0.000/DV, 0.000/DV, 0.000/DV,
		0.000/DV, 0.750/DV, 1.125/DV, 1.500/DV,
		1.875/DV, 2.250/DV, 2.625/DV, 3.000/DV,
	/* OCT 2 */
		0.000/DV, 0.000/DV, 0.000/DV, 0.000/DV,
		0.000/DV, 1.125/DV, 1.875/DV, 2.625/DV,
		3.000/DV, 3.750/DV, 4.125/DV, 4.500/DV,
		4.875/DV, 5.250/DV, 5.625/DV, 6.000/DV,
	/* OCT 3 */
		0.000/DV, 0.000/DV, 0.000/DV, 1.875/DV,
		3.000/DV, 4.125/DV, 4.875/DV, 5.625/DV,
		6.000/DV, 6.750/DV, 7.125/DV, 7.500/DV,
		7.875/DV, 8.250/DV, 8.625/DV, 9.000/DV,
	/* OCT 4 */
		0.000/DV, 0.000/DV, 3.000/DV, 4.875/DV,
		6.000/DV, 7.125/DV, 7.875/DV, 8.625/DV,
		9.000/DV, 9.750/DV,10.125/DV,10.500/DV,
		10.875/DV,11.250/DV,11.625/DV,12.000/DV,
	/* OCT 5 */
		0.000/DV, 3.000/DV, 6.000/DV, 7.875/DV,
		9.000/DV,10.125/DV,10.875/DV,11.625/DV,
		12.000/DV,12.750/DV,13.125/DV,13.500/DV,
		13.875/DV,14.250/DV,14.625/DV,15.000/DV,
	/* OCT 6 */
		0.000/DV, 6.000/DV, 9.000/DV,10.875/DV,
		12.000/DV,13.125/DV,13.875/DV,14.625/DV,
		15.000/DV,15.750/DV,16.125/DV,16.500/DV,
		16.875/DV,17.250/DV,17.625/DV,18.000/DV,
	/* OCT 7 */
		0.000/DV, 9.000/DV,12.000/DV,13.875/DV,
		15.000/DV,16.125/DV,16.875/DV,17.625/DV,
		18.000/DV,18.750/DV,19.125/DV,19.500/DV,
		19.875/DV,20.250/DV,20.625/DV,21.000/DV
};

/* 0 / 3.0 / 1.5 / 6.0 dB/OCT */
const uint32_t FM_OPL::ksl_shift[4] = { 31, 1, 2, 0 };


/* sustain level table (3dB per step) */
/* 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)*/
#define SC(db)			int32_t(db * (2.0 / ENV_STEP))
const int32_t FM_OPL::sl_tab[16]={
	SC( 0),SC( 1),SC( 2),SC( 3),SC( 4),SC( 5),SC( 6),SC( 7),
	SC( 8),SC( 9),SC(10),SC(11),SC(12),SC(13),SC(14),SC(31)
};


const unsigned char FM_OPL::eg_inc[15*RATE_STEPS]={
/*cycle:0 1  2 3  4 5  6 7*/

/* 0 */ 0,1, 0,1, 0,1, 0,1, /* rates 00..12 0 (increment by 0 or 1) */
/* 1 */ 0,1, 0,1, 1,1, 0,1, /* rates 00..12 1 */
/* 2 */ 0,1, 1,1, 0,1, 1,1, /* rates 00..12 2 */
/* 3 */ 0,1, 1,1, 1,1, 1,1, /* rates 00..12 3 */

/* 4 */ 1,1, 1,1, 1,1, 1,1, /* rate 13 0 (increment by 1) */
/* 5 */ 1,1, 1,2, 1,1, 1,2, /* rate 13 1 */
/* 6 */ 1,2, 1,2, 1,2, 1,2, /* rate 13 2 */
/* 7 */ 1,2, 2,2, 1,2, 2,2, /* rate 13 3 */

/* 8 */ 2,2, 2,2, 2,2, 2,2, /* rate 14 0 (increment by 2) */
/* 9 */ 2,2, 2,4, 2,2, 2,4, /* rate 14 1 */
/*10 */ 2,4, 2,4, 2,4, 2,4, /* rate 14 2 */
/*11 */ 2,4, 4,4, 2,4, 4,4, /* rate 14 3 */

/*12 */ 4,4, 4,4, 4,4, 4,4, /* rates 15 0, 15 1, 15 2, 15 3 (increment by 4) */
/*13 */ 8,8, 8,8, 8,8, 8,8, /* rates 15 2, 15 3 for attack */
/*14 */ 0,0, 0,0, 0,0, 0,0, /* infinity rates for attack and decay(s) */
};


#define O(a) (a*RATE_STEPS)

/*note that there is no O(13) in this table - it's directly in the code */
const unsigned char eg_rate_select[16+64+16]={   /* Envelope Generator rates (16 + 64 rates + 16 RKS) */
/* 16 infinite time rates */
O(14),O(14),O(14),O(14),O(14),O(14),O(14),O(14),
O(14),O(14),O(14),O(14),O(14),O(14),O(14),O(14),

/* rates 00-12 */
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),

/* rate 13 */
O( 4),O( 5),O( 6),O( 7),

/* rate 14 */
O( 8),O( 9),O(10),O(11),

/* rate 15 */
O(12),O(12),O(12),O(12),

/* 16 dummy rates (same as 15 3) */
O(12),O(12),O(12),O(12),O(12),O(12),O(12),O(12),
O(12),O(12),O(12),O(12),O(12),O(12),O(12),O(12),

};
#undef O

/*rate  0,    1,    2,    3,   4,   5,   6,  7,  8,  9,  10, 11, 12, 13, 14, 15 */
/*shift 12,   11,   10,   9,   8,   7,   6,  5,  4,  3,  2,  1,  0,  0,  0,  0  */
/*mask  4095, 2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3,  1,  0,  0,  0,  0  */

#define O(a) (a*1)
const unsigned char eg_rate_shift[16+64+16]={    /* Envelope Generator counter shifts (16 + 64 rates + 16 RKS) */
/* 16 infinite time rates */
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),

/* rates 00-12 */
O(12),O(12),O(12),O(12),
O(11),O(11),O(11),O(11),
O(10),O(10),O(10),O(10),
O( 9),O( 9),O( 9),O( 9),
O( 8),O( 8),O( 8),O( 8),
O( 7),O( 7),O( 7),O( 7),
O( 6),O( 6),O( 6),O( 6),
O( 5),O( 5),O( 5),O( 5),
O( 4),O( 4),O( 4),O( 4),
O( 3),O( 3),O( 3),O( 3),
O( 2),O( 2),O( 2),O( 2),
O( 1),O( 1),O( 1),O( 1),
O( 0),O( 0),O( 0),O( 0),

/* rate 13 */
O( 0),O( 0),O( 0),O( 0),

/* rate 14 */
O( 0),O( 0),O( 0),O( 0),

/* rate 15 */
O( 0),O( 0),O( 0),O( 0),

/* 16 dummy rates (same as 15 3) */
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),

};
#undef O


/* multiple table */
#define ML 2
const uint8_t FM_OPL::mul_tab[16]= {
/* 1/2, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,10,12,12,15,15 */
	ML/2, 1*ML, 2*ML, 3*ML, 4*ML, 5*ML, 6*ML, 7*ML,
	8*ML, 9*ML,10*ML,10*ML,12*ML,12*ML,15*ML,15*ML
};
#undef ML

signed int FM_OPL::tl_tab[TL_TAB_LEN];

/* sin waveform table in 'decibel' scale */
/* four waveforms on OPL2 type chips */
unsigned int FM_OPL::sin_tab[SIN_LEN * 4];


/* LFO Amplitude Modulation table (verified on real YM3812)
   27 output levels (triangle waveform); 1 level takes one of: 192, 256 or 448 samples

   Length: 210 elements.

    Each of the elements has to be repeated
    exactly 64 times (on 64 consecutive samples).
    The whole table takes: 64 * 210 = 13440 samples.

    When AM = 1 data is used directly
    When AM = 0 data is divided by 4 before being used (losing precision is important)
*/

const uint8_t FM_OPL::lfo_am_table[LFO_AM_TAB_ELEMENTS] = {
0,0,0,0,0,0,0,
1,1,1,1,
2,2,2,2,
3,3,3,3,
4,4,4,4,
5,5,5,5,
6,6,6,6,
7,7,7,7,
8,8,8,8,
9,9,9,9,
10,10,10,10,
11,11,11,11,
12,12,12,12,
13,13,13,13,
14,14,14,14,
15,15,15,15,
16,16,16,16,
17,17,17,17,
18,18,18,18,
19,19,19,19,
20,20,20,20,
21,21,21,21,
22,22,22,22,
23,23,23,23,
24,24,24,24,
25,25,25,25,
26,26,26,
25,25,25,25,
24,24,24,24,
23,23,23,23,
22,22,22,22,
21,21,21,21,
20,20,20,20,
19,19,19,19,
18,18,18,18,
17,17,17,17,
16,16,16,16,
15,15,15,15,
14,14,14,14,
13,13,13,13,
12,12,12,12,
11,11,11,11,
10,10,10,10,
9,9,9,9,
8,8,8,8,
7,7,7,7,
6,6,6,6,
5,5,5,5,
4,4,4,4,
3,3,3,3,
2,2,2,2,
1,1,1,1
};

/* LFO Phase Modulation table (verified on real YM3812) */
const int8_t FM_OPL::lfo_pm_table[8*8*2] = {
/* FNUM2/FNUM = 00 0xxxxxxx (0x0000) */
0, 0, 0, 0, 0, 0, 0, 0, /*LFO PM depth = 0*/
0, 0, 0, 0, 0, 0, 0, 0, /*LFO PM depth = 1*/

/* FNUM2/FNUM = 00 1xxxxxxx (0x0080) */
0, 0, 0, 0, 0, 0, 0, 0, /*LFO PM depth = 0*/
1, 0, 0, 0,-1, 0, 0, 0, /*LFO PM depth = 1*/

/* FNUM2/FNUM = 01 0xxxxxxx (0x0100) */
1, 0, 0, 0,-1, 0, 0, 0, /*LFO PM depth = 0*/
2, 1, 0,-1,-2,-1, 0, 1, /*LFO PM depth = 1*/

/* FNUM2/FNUM = 01 1xxxxxxx (0x0180) */
1, 0, 0, 0,-1, 0, 0, 0, /*LFO PM depth = 0*/
3, 1, 0,-1,-3,-1, 0, 1, /*LFO PM depth = 1*/

/* FNUM2/FNUM = 10 0xxxxxxx (0x0200) */
2, 1, 0,-1,-2,-1, 0, 1, /*LFO PM depth = 0*/
4, 2, 0,-2,-4,-2, 0, 2, /*LFO PM depth = 1*/

/* FNUM2/FNUM = 10 1xxxxxxx (0x0280) */
2, 1, 0,-1,-2,-1, 0, 1, /*LFO PM depth = 0*/
5, 2, 0,-2,-5,-2, 0, 2, /*LFO PM depth = 1*/

/* FNUM2/FNUM = 11 0xxxxxxx (0x0300) */
3, 1, 0,-1,-3,-1, 0, 1, /*LFO PM depth = 0*/
6, 3, 0,-3,-6,-3, 0, 3, /*LFO PM depth = 1*/

/* FNUM2/FNUM = 11 1xxxxxxx (0x0380) */
3, 1, 0,-1,-3,-1, 0, 1, /*LFO PM depth = 0*/
7, 3, 0,-3,-7,-3, 0, 3  /*LFO PM depth = 1*/
};


/* lock level of common table */
int FM_OPL::num_lock = 0;



static inline int limit( int val, int max, int min ) {
	if ( val > max )
		val = max;
	else if ( val < min )
		val = min;

	return val;
}


/* generic table initialize */
int FM_OPL::init_tables()
{
	signed int i,x;
	signed int n;
	double o,m;


	for (x=0; x<TL_RES_LEN; x++)
	{
		m = (1<<16) / pow(2, (x+1) * (ENV_STEP/4.0) / 8.0);
		m = floor(m);

		/* we never reach (1<<16) here due to the (x+1) */
		/* result fits within 16 bits at maximum */

		n = (int)m;     /* 16 bits here */
		n >>= 4;        /* 12 bits here */
		if (n&1)        /* round to nearest */
			n = (n>>1)+1;
		else
			n = n>>1;
						/* 11 bits here (rounded) */
		n <<= 1;        /* 12 bits here (as in real chip) */
		tl_tab[ x*2 + 0 ] = n;
		tl_tab[ x*2 + 1 ] = -tl_tab[ x*2 + 0 ];

		for (i=1; i<12; i++)
		{
			tl_tab[ x*2+0 + i*2*TL_RES_LEN ] =  tl_tab[ x*2+0 ]>>i;
			tl_tab[ x*2+1 + i*2*TL_RES_LEN ] = -tl_tab[ x*2+0 + i*2*TL_RES_LEN ];
		}
	#if 0
			logerror("tl %04i", x*2);
			for (i=0; i<12; i++)
				logerror(", [%02i] %5i", i*2, tl_tab[ x*2 /*+1*/ + i*2*TL_RES_LEN ] );
			logerror("\n");
	#endif
	}
	/*logerror("FMOPL.C: TL_TAB_LEN = %i elements (%i bytes)\n",TL_TAB_LEN, (int)sizeof(tl_tab));*/


	for (i=0; i<SIN_LEN; i++)
	{
		/* non-standard sinus */
		m = sin( ((i*2)+1) * M_PI / SIN_LEN ); /* checked against the real chip */

		/* we never reach zero here due to ((i*2)+1) */

		if (m>0.0)
			o = 8*log(1.0/m)/log(2.0);  /* convert to 'decibels' */
		else
			o = 8*log(-1.0/m)/log(2.0); /* convert to 'decibels' */

		o = o / (ENV_STEP/4);

		n = (int)(2.0*o);
		if (n&1)                        /* round to nearest */
			n = (n>>1)+1;
		else
			n = n>>1;

		sin_tab[ i ] = n*2 + (m>=0.0? 0: 1 );

		/*logerror("FMOPL.C: sin [%4i (hex=%03x)]= %4i (tl_tab value=%5i)\n", i, i, sin_tab[i], tl_tab[sin_tab[i]] );*/
	}

	for (i=0; i<SIN_LEN; i++)
	{
		/* waveform 1:  __      __     */
		/*             /  \____/  \____*/
		/* output only first half of the sinus waveform (positive one) */

		if (i & (1<<(SIN_BITS-1)) )
			sin_tab[1*SIN_LEN+i] = TL_TAB_LEN;
		else
			sin_tab[1*SIN_LEN+i] = sin_tab[i];

		/* waveform 2:  __  __  __  __ */
		/*             /  \/  \/  \/  \*/
		/* abs(sin) */

		sin_tab[2*SIN_LEN+i] = sin_tab[i & (SIN_MASK>>1) ];

		/* waveform 3:  _   _   _   _  */
		/*             / |_/ |_/ |_/ |_*/
		/* abs(output only first quarter of the sinus waveform) */

		if (i & (1<<(SIN_BITS-2)) )
			sin_tab[3*SIN_LEN+i] = TL_TAB_LEN;
		else
			sin_tab[3*SIN_LEN+i] = sin_tab[i & (SIN_MASK>>2)];

		/*logerror("FMOPL.C: sin1[%4i]= %4i (tl_tab value=%5i)\n", i, sin_tab[1*SIN_LEN+i], tl_tab[sin_tab[1*SIN_LEN+i]] );
		logerror("FMOPL.C: sin2[%4i]= %4i (tl_tab value=%5i)\n", i, sin_tab[2*SIN_LEN+i], tl_tab[sin_tab[2*SIN_LEN+i]] );
		logerror("FMOPL.C: sin3[%4i]= %4i (tl_tab value=%5i)\n", i, sin_tab[3*SIN_LEN+i], tl_tab[sin_tab[3*SIN_LEN+i]] );*/
	}
	/*logerror("FMOPL.C: ENV_QUIET= %08x (dec*8=%i)\n", ENV_QUIET, ENV_QUIET*8 );*/


#ifdef SAVE_SAMPLE
	sample[0]=fopen("sampsum.pcm","wb");
#endif

	return 1;
}


void FM_OPL::initialize()
{
	int i;

	/* frequency base */
	freqbase  = (rate) ? ((double)clock / 72.0) / rate  : 0;
#if 0
	rate = (double)clock / 72.0;
	freqbase  = 1.0;
#endif

	/*logerror("freqbase=%f\n", freqbase);*/

	/* Timer base time */
	//TimerBase = attotime::from_hz(clock) * 72;

	/* make fnumber -> increment counter table */
	for( i=0 ; i < 1024 ; i++ )
	{
		/* opn phase increment counter = 20bit */
		fn_tab[i] = (uint32_t)( (double)i * 64 * freqbase * (1<<(FREQ_SH-10)) ); /* -10 because chip works with 10.10 fixed point, while we use 16.16 */
#if 0
		logerror("FMOPL.C: fn_tab[%4i] = %08x (dec=%8i)\n",
					i, fn_tab[i]>>6, fn_tab[i]>>6 );
#endif
	}

#if 0
	for( i=0 ; i < 16 ; i++ )
	{
		logerror("FMOPL.C: sl_tab[%i] = %08x\n",
			i, sl_tab[i] );
	}
	for( i=0 ; i < 8 ; i++ )
	{
		int j;
		logerror("FMOPL.C: ksl_tab[oct=%2i] =",i);
		for (j=0; j<16; j++)
		{
			logerror("%08x ", static_cast<uint32_t>(ksl_tab[i*16+j]) );
		}
		logerror("\n");
	}
#endif


	/* Amplitude modulation: 27 output levels (triangle waveform); 1 level takes one of: 192, 256 or 448 samples */
	/* One entry from LFO_AM_TABLE lasts for 64 samples */
	lfo_am_inc = (uint32_t)((1.0 / 64.0 ) * (1<<LFO_SH) * freqbase);

	/* Vibrato: 8 output levels (triangle waveform); 1 level takes 1024 samples */
	lfo_pm_inc = (uint32_t)((1.0 / 1024.0) * (1<<LFO_SH) * freqbase);

	/*logerror ("lfo_am_inc = %8x ; lfo_pm_inc = %8x\n", lfo_am_inc, lfo_pm_inc);*/

	/* Noise generator: a step takes 1 sample */
	noise_f = (uint32_t)((1.0 / 1.0) * (1<<FREQ_SH) * freqbase);

	eg_timer_add = (uint32_t)((1<<EG_SH) * freqbase);
	eg_timer_overflow = ( 1 ) * (1<<EG_SH);
	/*logerror("OPLinit eg_timer_add=%8x eg_timer_overflow=%8x\n", eg_timer_add, eg_timer_overflow);*/
}


/* write a value v to register r on OPL chip */
void FM_OPL::WriteReg(int r, int v)
{
	OPL_CH *CH;
	int slot;
	int block_fnum;


	/* adjust bus to 8 bits */
	r &= 0xff;
	v &= 0xff;

	switch(r&0xe0)
	{
	case 0x00:  /* 00-1f:control */
		switch(r&0x1f)
		{
		case 0x01:  /* waveform select enable */
			if(type&OPL_TYPE_WAVESEL)
			{
				wavesel = v&0x20;
				/* do not change the waveform previously selected */
			}
			break;
		case 0x02:  /* Timer 1 */
			T[0] = (256-v)*4;
			break;
		case 0x03:  /* Timer 2 */
			T[1] = (256-v)*16;
			break;
#if 0
		case 0x04:  /* IRQ clear / mask and Timer enable */
			if(v&0x80)
			{   /* IRQ flag clear */
				STATUS_RESET(0x7f-0x08); /* don't reset BFRDY flag or we will have to call deltat module to set the flag */
			}
			else
			{   /* set IRQ mask ,timer enable*/
				uint8_t st1 = v&1;
				uint8_t st2 = (v>>1)&1;

				/* IRQRST,T1MSK,t2MSK,EOSMSK,BRMSK,x,ST2,ST1 */
				STATUS_RESET(v & (0x78-0x08));
				STATUSMASK_SET((~v) & 0x78);

				/* timer 2 */
				if(st[1] != st2)
				{
					attotime period = st2 ? (TimerBase * T[1]) : attotime::zero;
					st[1] = st2;
					if (timer_handler) (timer_handler)(TimerParam,1,period);
				}
				/* timer 1 */
				if(st[0] != st1)
				{
					attotime period = st1 ? (TimerBase * T[0]) : attotime::zero;
					st[0] = st1;
					if (timer_handler) (timer_handler)(TimerParam,0,period);
				}
			}
			break;
#endif
#if BUILD_Y8950
		case 0x06:      /* Key Board OUT */
			if(type&OPL_TYPE_KEYBOARD)
			{
				if(keyboardhandler_w)
					keyboardhandler_w(keyboard_param,v);
				else
					device->logerror("Y8950: write unmapped KEYBOARD port\n");
			}
			break;
		case 0x07:  /* DELTA-T control 1 : START,REC,MEMDATA,REPT,SPOFF,x,x,RST */
			if(type&OPL_TYPE_ADPCM)
				deltat->ADPCM_Write(r-0x07,v);
			break;
#endif
		case 0x08:  /* MODE,DELTA-T control 2 : CSM,NOTESEL,x,x,smpl,da/ad,64k,rom */
			mode = v;
#if BUILD_Y8950
			if(type&OPL_TYPE_ADPCM)
				deltat->ADPCM_Write(r-0x07,v&0x0f); /* mask 4 LSBs in register 08 for DELTA-T unit */
#endif
			break;

#if BUILD_Y8950
		case 0x09:      /* START ADD */
		case 0x0a:
		case 0x0b:      /* STOP ADD  */
		case 0x0c:
		case 0x0d:      /* PRESCALE   */
		case 0x0e:
		case 0x0f:      /* ADPCM data write */
		case 0x10:      /* DELTA-N    */
		case 0x11:      /* DELTA-N    */
		case 0x12:      /* ADPCM volume */
			if(type&OPL_TYPE_ADPCM)
				deltat->ADPCM_Write(r-0x07,v);
			break;

		case 0x15:      /* DAC data high 8 bits (F7,F6...F2) */
		case 0x16:      /* DAC data low 2 bits (F1, F0 in bits 7,6) */
		case 0x17:      /* DAC data shift (S2,S1,S0 in bits 2,1,0) */
			device->logerror("FMOPL.C: DAC data register written, but not implemented reg=%02x val=%02x\n",r,v);
			break;

		case 0x18:      /* I/O CTRL (Direction) */
			if(type&OPL_TYPE_IO)
				portDirection = v&0x0f;
			break;
		case 0x19:      /* I/O DATA */
			if(type&OPL_TYPE_IO)
			{
				portLatch = v;
				if(porthandler_w)
					porthandler_w(port_param,v&portDirection);
			}
			break;
#endif
		default:
			device->logerror("FMOPL.C: write to unknown register: %02x\n",r);
			break;
		}
		break;
	case 0x20:  /* am ON, vib ON, ksr, eg_type, mul */
		slot = slot_array[r&0x1f];
		if(slot < 0) return;
		set_mul(slot,v);
		break;
	case 0x40:
		slot = slot_array[r&0x1f];
		if(slot < 0) return;
		set_ksl_tl(slot,v);
		break;
	case 0x60:
		slot = slot_array[r&0x1f];
		if(slot < 0) return;
		set_ar_dr(slot,v);
		break;
	case 0x80:
		slot = slot_array[r&0x1f];
		if(slot < 0) return;
		set_sl_rr(slot,v);
		break;
	case 0xa0:
		if (r == 0xbd)          /* am depth, vibrato depth, r,bd,sd,tom,tc,hh */
		{
			lfo_am_depth = v & 0x80;
			lfo_pm_depth_range = (v&0x40) ? 8 : 0;

			rhythm  = v&0x3f;

			if(rhythm&0x20)
			{
				/* BD key on/off */
				if(v&0x10)
				{
					P_CH[6].SLOT[SLOT1].KEYON(2);
					P_CH[6].SLOT[SLOT2].KEYON(2);
				}
				else
				{
					P_CH[6].SLOT[SLOT1].KEYOFF(~2);
					P_CH[6].SLOT[SLOT2].KEYOFF(~2);
				}
				/* HH key on/off */
				if(v&0x01) P_CH[7].SLOT[SLOT1].KEYON ( 2);
				else       P_CH[7].SLOT[SLOT1].KEYOFF(~2);
				/* SD key on/off */
				if(v&0x08) P_CH[7].SLOT[SLOT2].KEYON ( 2);
				else       P_CH[7].SLOT[SLOT2].KEYOFF(~2);
				/* TOM key on/off */
				if(v&0x04) P_CH[8].SLOT[SLOT1].KEYON ( 2);
				else       P_CH[8].SLOT[SLOT1].KEYOFF(~2);
				/* TOP-CY key on/off */
				if(v&0x02) P_CH[8].SLOT[SLOT2].KEYON ( 2);
				else       P_CH[8].SLOT[SLOT2].KEYOFF(~2);
			}
			else
			{
				/* BD key off */
				P_CH[6].SLOT[SLOT1].KEYOFF(~2);
				P_CH[6].SLOT[SLOT2].KEYOFF(~2);
				/* HH key off */
				P_CH[7].SLOT[SLOT1].KEYOFF(~2);
				/* SD key off */
				P_CH[7].SLOT[SLOT2].KEYOFF(~2);
				/* TOM key off */
				P_CH[8].SLOT[SLOT1].KEYOFF(~2);
				/* TOP-CY off */
				P_CH[8].SLOT[SLOT2].KEYOFF(~2);
			}
			return;
		}
		/* keyon,block,fnum */
		if( (r&0x0f) > 8) return;
		CH = &P_CH[r&0x0f];
		if(!(r&0x10))
		{   /* a0-a8 */
			block_fnum  = (CH->block_fnum&0x1f00) | v;
		}
		else
		{   /* b0-b8 */
			block_fnum = ((v&0x1f)<<8) | (CH->block_fnum&0xff);

			if(v&0x20)
			{
				CH->SLOT[SLOT1].KEYON ( 1);
				CH->SLOT[SLOT2].KEYON ( 1);
			}
			else
			{
				CH->SLOT[SLOT1].KEYOFF(~1);
				CH->SLOT[SLOT2].KEYOFF(~1);
			}
		}
		/* update */
		if(CH->block_fnum != (unsigned int)block_fnum)
		{
			uint8_t block  = block_fnum >> 10;

			CH->block_fnum = block_fnum;

			CH->ksl_base = static_cast<uint32_t>(ksl_tab[block_fnum>>6]);
			CH->fc       = fn_tab[block_fnum&0x03ff] >> (7-block);

			/* BLK 2,1,0 bits -> bits 3,2,1 of kcode */
			CH->kcode    = (CH->block_fnum&0x1c00)>>9;

				/* the info below is actually opposite to what is stated in the Manuals (verified on real YM3812) */
			/* if notesel == 0 -> lsb of kcode is bit 10 (MSB) of fnum  */
			/* if notesel == 1 -> lsb of kcode is bit 9 (MSB-1) of fnum */
			if (mode&0x40)
				CH->kcode |= (CH->block_fnum&0x100)>>8; /* notesel == 1 */
			else
				CH->kcode |= (CH->block_fnum&0x200)>>9; /* notesel == 0 */

			/* refresh Total Level in both SLOTs of this channel */
			CH->SLOT[SLOT1].TLL = CH->SLOT[SLOT1].TL + (CH->ksl_base>>CH->SLOT[SLOT1].ksl);
			CH->SLOT[SLOT2].TLL = CH->SLOT[SLOT2].TL + (CH->ksl_base>>CH->SLOT[SLOT2].ksl);

			/* refresh frequency counter in both SLOTs of this channel */
			CH->CALC_FCSLOT(CH->SLOT[SLOT1]);
			CH->CALC_FCSLOT(CH->SLOT[SLOT2]);
		}
		break;
	case 0xc0:
		/* FB,C */
		if( (r&0x0f) > 8) return;
		CH = &P_CH[r&0x0f];
		CH->SLOT[SLOT1].FB  = (v>>1)&7 ? ((v>>1)&7) + 7 : 0;
		CH->SLOT[SLOT1].CON = v&1;
		CH->SLOT[SLOT1].connect1 = CH->SLOT[SLOT1].CON ? &output[0] : &phase_modulation;
		break;
	case 0xe0: /* waveform select */
		/* simply ignore write to the waveform select register if selecting not enabled in test register */
		if(wavesel)
		{
			slot = slot_array[r&0x1f];
			if(slot < 0) return;
			CH = &P_CH[slot/2];

			CH->SLOT[slot&1].wavetable = (v&0x03)*SIN_LEN;
		}
		break;
	}
}


void FM_OPL::ResetChip()
{
	eg_timer = 0;
	eg_cnt   = 0;

	noise_rng = 1; /* noise shift register */
	mode   = 0;    /* normal mode */
	STATUS_RESET(0x7f);

	/* reset with register write */
	WriteReg(0x01,0); /* wavesel disable */
	WriteReg(0x02,0); /* Timer1 */
	WriteReg(0x03,0); /* Timer2 */
	WriteReg(0x04,0); /* IRQ mask clear */
	for(int i = 0xff ; i >= 0x20 ; i-- ) WriteReg(i,0);

	/* reset operator parameters */
//	for(OPL_CH &CH : P_CH)
	for(unsigned int ch = 0; ch < sizeof( P_CH )/ sizeof(P_CH[0]); ch++)
	{
		OPL_CH &CH = P_CH[ch];
//		for(OPL_SLOT &SLOT : CH.SLOT)
		for(unsigned int slot = 0; slot < sizeof( CH.SLOT ) / sizeof( CH.SLOT[0]); slot++)
		{
		    
			OPL_SLOT &SLOT = CH.SLOT[slot];
			/* wave table */
			SLOT.wavetable = 0;
			SLOT.state     = EG_OFF;
			SLOT.volume    = MAX_ATT_INDEX;
		}
	}
#if BUILD_Y8950
	if(type&OPL_TYPE_ADPCM)
	{
		YM_DELTAT *DELTAT = deltat;

		DELTAT->freqbase = freqbase;
		DELTAT->output_pointer = &output_deltat[0];
		DELTAT->portshift = 5;
		DELTAT->output_range = 1<<23;
		DELTAT->ADPCM_Reset(0,YM_DELTAT::EMULATION_MODE_NORMAL,device);
	}
#endif
}

#if 0/*not used*/
void FM_OPL::postload()
{
	for(unsigned int ch = 0; ch < sizeof( P_CH )/ sizeof(P_CH[0]); ch++)
	{
		OPL_CH &CH = P_CH[ch];
		/* Look up key scale level */
		uint32_t const block_fnum = CH.block_fnum;
		CH.ksl_base = static_cast<uint32_t>(ksl_tab[block_fnum >> 6]);
		CH.fc       = fn_tab[block_fnum & 0x03ff] >> (7 - (block_fnum >> 10));

		for(unsigned int slot = 0; slot < sizeof( CH.SLOT ) / sizeof( CH.SLOT[0]); slot++)
		{
			OPL_SLOT &SLOT = CH.SLOT[slot];
			/* Calculate key scale rate */
			SLOT.ksr = CH.kcode >> SLOT.KSR;

			/* Calculate attack, decay and release rates */
			if ((SLOT.ar + SLOT.ksr) < 16+62)
			{
				SLOT.eg_sh_ar  = eg_rate_shift [SLOT.ar + SLOT.ksr ];
				SLOT.eg_sel_ar = eg_rate_select[SLOT.ar + SLOT.ksr ];
			}
			else
			{
				SLOT.eg_sh_ar  = 0;
				SLOT.eg_sel_ar = 13*RATE_STEPS;
			}
			SLOT.eg_sh_dr  = eg_rate_shift [SLOT.dr + SLOT.ksr ];
			SLOT.eg_sel_dr = eg_rate_select[SLOT.dr + SLOT.ksr ];
			SLOT.eg_sh_rr  = eg_rate_shift [SLOT.rr + SLOT.ksr ];
			SLOT.eg_sel_rr = eg_rate_select[SLOT.rr + SLOT.ksr ];

			/* Calculate phase increment */
			SLOT.Incr = CH.fc * SLOT.mul;

			/* Total level */
			SLOT.TLL = SLOT.TL + (CH.ksl_base >> SLOT.ksl);

			/* Connect output */
			SLOT.connect1 = SLOT.CON ? &output[0] : &phase_modulation;
		}
	}
#if BUILD_Y8950
	if ( (type & OPL_TYPE_ADPCM) && (deltat) )
	{
		// We really should call the postlod function for the YM_DELTAT, but it's hard without registers
		// (see the way the YM2610 does it)
		//deltat->postload(REGS);
	}
#endif
}
#endif /* end not used */

} // anonymous namespace


#if  0
static void OPLsave_state_channel(device_t *device, OPL_CH *CH)
{
	int slot, ch;

	for( ch=0 ; ch < 9 ; ch++, CH++ )
	{
		/* channel */
		device->save_item(NAME(CH->block_fnum), ch);
		device->save_item(NAME(CH->kcode), ch);
		/* slots */
		for( slot=0 ; slot < 2 ; slot++ )
		{
			OPL_SLOT *SLOT = &CH->SLOT[slot];

			device->save_item(NAME(SLOT->ar), ch * 2 + slot);
			device->save_item(NAME(SLOT->dr), ch * 2 + slot);
			device->save_item(NAME(SLOT->rr), ch * 2 + slot);
			device->save_item(NAME(SLOT->KSR), ch * 2 + slot);
			device->save_item(NAME(SLOT->ksl), ch * 2 + slot);
			device->save_item(NAME(SLOT->mul), ch * 2 + slot);

			device->save_item(NAME(SLOT->Cnt), ch * 2 + slot);
			device->save_item(NAME(SLOT->FB), ch * 2 + slot);
			device->save_item(NAME(SLOT->op1_out), ch * 2 + slot);
			device->save_item(NAME(SLOT->CON), ch * 2 + slot);

			device->save_item(NAME(SLOT->eg_type), ch * 2 + slot);
			device->save_item(NAME(SLOT->state), ch * 2 + slot);
			device->save_item(NAME(SLOT->TL), ch * 2 + slot);
			device->save_item(NAME(SLOT->volume), ch * 2 + slot);
			device->save_item(NAME(SLOT->sl), ch * 2 + slot);
			device->save_item(NAME(SLOT->key), ch * 2 + slot);

			device->save_item(NAME(SLOT->AMmask), ch * 2 + slot);
			device->save_item(NAME(SLOT->vib), ch * 2 + slot);

			device->save_item(NAME(SLOT->wavetable), ch * 2 + slot);
		}
	}
}

/* Register savestate for a virtual YM3812/YM3526Y8950 */

static void OPL_save_state(FM_OPL *OPL, device_t *device)
{
	OPLsave_state_channel(device, OPL->P_CH);

	device->save_item(NAME(OPL->eg_cnt));
	device->save_item(NAME(OPL->eg_timer));

	device->save_item(NAME(OPL->rhythm));

	device->save_item(NAME(OPL->lfo_am_depth));
	device->save_item(NAME(OPL->lfo_pm_depth_range));
	device->save_item(NAME(OPL->lfo_am_cnt));
	device->save_item(NAME(OPL->lfo_pm_cnt));

	device->save_item(NAME(OPL->noise_rng));
	device->save_item(NAME(OPL->noise_p));

	if( OPL->type & OPL_TYPE_WAVESEL )
	{
		device->save_item(NAME(OPL->wavesel));
	}

	device->save_item(NAME(OPL->T));
	device->save_item(NAME(OPL->st));

#if BUILD_Y8950
	if ( (OPL->type & OPL_TYPE_ADPCM) && (OPL->deltat) )
	{
		OPL->deltat->savestate(device);
	}

	if ( OPL->type & OPL_TYPE_IO )
	{
		device->save_item(NAME(OPL->portDirection));
		device->save_item(NAME(OPL->portLatch));
	}
#endif

	device->save_item(NAME(OPL->address));
	device->save_item(NAME(OPL->status));
	device->save_item(NAME(OPL->statusmask));
	device->save_item(NAME(OPL->mode));

	device->machine().save().register_postload(save_prepost_delegate(FUNC(FM_OPL::postload), OPL));
}

#endif

static void OPL_clock_changed(FM_OPL *OPL, uint32_t clock, uint32_t rate)
{
	OPL->clock = clock;
	OPL->rate  = rate;

	/* init global tables */
	OPL->initialize();
}


/* Create one of virtual YM3812/YM3526/Y8950 */
/* 'clock' is chip clock in Hz  */
/* 'rate'  is sampling rate  */
static FM_OPL *OPLCreate(device_t *device, uint32_t clock, uint32_t rate, int type)
{
	char *ptr;
	FM_OPL *OPL;
	int state_size;

	if (FM_OPL::LockTable(device) == -1) return nullptr;

	/* calculate OPL state size */
	state_size  = sizeof(FM_OPL);

#if BUILD_Y8950
	if (type&OPL_TYPE_ADPCM) state_size+= sizeof(YM_DELTAT);
#endif

	/* allocate memory block */
	ptr = (char *)auto_alloc_array_clear(device->machine(), uint8_t, state_size);

	OPL  = (FM_OPL *)ptr;

	ptr += sizeof(FM_OPL);

#if BUILD_Y8950
	if (type&OPL_TYPE_ADPCM)
	{
		OPL->deltat = (YM_DELTAT *)ptr;
	}
	ptr += sizeof(YM_DELTAT);
#endif

	OPL->device = device;
	OPL->type  = type;
	OPL_clock_changed(OPL, clock, rate);

	return OPL;
}

/* Destroy one of virtual YM3812 */
static void OPLDestroy(FM_OPL *OPL)
{
	FM_OPL::UnLockTable();
    free(OPL);
}

/* Optional handlers */

static void OPLSetTimerHandler(FM_OPL *OPL,OPL_TIMERHANDLER timer_handler,device_t *device)
{
	OPL->timer_handler   = timer_handler;
	OPL->TimerParam = device;
}
static void OPLSetIRQHandler(FM_OPL *OPL,OPL_IRQHANDLER IRQHandler,device_t *device)
{
	OPL->IRQHandler     = IRQHandler;
	OPL->IRQParam = device;
}
static void OPLSetUpdateHandler(FM_OPL *OPL,OPL_UPDATEHANDLER UpdateHandler,device_t *device)
{
	OPL->UpdateHandler = UpdateHandler;
	OPL->UpdateParam = device;
}

static int OPLWrite(FM_OPL *OPL,int a,int v)
{
	if( !(a&1) )
	{   /* address port */
		OPL->address = v & 0xff;
	}
	else
	{   /* data port */
		if(OPL->UpdateHandler) OPL->UpdateHandler(OPL->UpdateParam,0);
		OPL->WriteReg(OPL->address,v);
	}
	return OPL->status>>7;
}

static unsigned char OPLRead(FM_OPL *OPL,int a)
{
	if( !(a&1) )
	{
		/* status port */

		#if BUILD_Y8950

		if(OPL->type&OPL_TYPE_ADPCM)    /* Y8950 */
		{
			return (OPL->status & (OPL->statusmask|0x80)) | (OPL->deltat->PCM_BSY&1);
		}

		#endif

		/* OPL and OPL2 */
		return OPL->status & (OPL->statusmask|0x80);
	}

#if BUILD_Y8950
	/* data port */
	switch(OPL->address)
	{
	case 0x05: /* KeyBoard IN */
		if(OPL->type&OPL_TYPE_KEYBOARD)
		{
			if(OPL->keyboardhandler_r)
				return OPL->keyboardhandler_r(OPL->keyboard_param);
			else
				OPL->device->logerror("Y8950: read unmapped KEYBOARD port\n");
		}
		return 0;

	case 0x0f: /* ADPCM-DATA  */
		if(OPL->type&OPL_TYPE_ADPCM)
		{
			uint8_t val;

			val = OPL->deltat->ADPCM_Read();
			/*logerror("Y8950: read ADPCM value read=%02x\n",val);*/
			return val;
		}
		return 0;

	case 0x19: /* I/O DATA    */
		if(OPL->type&OPL_TYPE_IO)
		{
			if(OPL->porthandler_r)
				return OPL->porthandler_r(OPL->port_param);
			else
				OPL->device->logerror("Y8950:read unmapped I/O port\n");
		}
		return 0;
	case 0x1a: /* PCM-DATA    */
		if(OPL->type&OPL_TYPE_ADPCM)
		{
			OPL->device->logerror("Y8950 A/D conversion is accessed but not implemented !\n");
			return 0x80; /* 2's complement PCM data - result from A/D conversion */
		}
		return 0;
	}
#endif

	return 0xff;
}

/* CSM Key Control */
static inline void CSMKeyControll(OPL_CH *CH)
{
	CH->SLOT[SLOT1].KEYON(4);
	CH->SLOT[SLOT2].KEYON(4);

	/* The key off should happen exactly one sample later - not implemented correctly yet */

	CH->SLOT[SLOT1].KEYOFF(~4);
	CH->SLOT[SLOT2].KEYOFF(~4);
}

static int OPLTimerOver(FM_OPL *OPL,int c)
{
	if( c )
	{   /* Timer B */
		OPL->STATUS_SET(0x20);
	}
	else
	{   /* Timer A */
		OPL->STATUS_SET(0x40);
		/* CSM mode key,TL control */
		if( OPL->mode & 0x80 )
		{   /* CSM mode total level latch and auto key on */
			int ch;
			if(OPL->UpdateHandler) OPL->UpdateHandler(OPL->UpdateParam,0);
			for(ch=0; ch<9; ch++)
				CSMKeyControll( &OPL->P_CH[ch] );
		}
	}
	/* reload timer */
	//if (OPL->timer_handler) (OPL->timer_handler)(OPL->TimerParam,c,OPL->TimerBase * OPL->T[c]);
	return OPL->status>>7;
}

#define MAX_OPL_CHIPS 2


#if BUILD_YM3812

void ym3812_clock_changed(void *chip, uint32_t clock, uint32_t rate)
{
	OPL_clock_changed((FM_OPL *)chip, clock, rate);
}

void * ym3812_init(device_t *device, uint32_t clock, uint32_t rate)
{
	/* emulator create */
	FM_OPL *YM3812 = OPLCreate(device,clock,rate,OPL_TYPE_YM3812);
	if (YM3812)
	{
		//OPL_save_state(YM3812, device);
		ym3812_reset_chip(YM3812);
	}
	return YM3812;
}

void ym3812_shutdown(void *chip)
{
	FM_OPL *YM3812 = (FM_OPL *)chip;

	/* emulator shutdown */
	OPLDestroy(YM3812);
}
void ym3812_reset_chip(void *chip)
{
	FM_OPL *YM3812 = (FM_OPL *)chip;
	YM3812->ResetChip();
}

int ym3812_write(void *chip, int a, int v)
{
	FM_OPL *YM3812 = (FM_OPL *)chip;
	return OPLWrite(YM3812, a, v);
}

unsigned char ym3812_read(void *chip, int a)
{
	FM_OPL *YM3812 = (FM_OPL *)chip;
	/* YM3812 always returns bit2 and bit1 in HIGH state */
	return OPLRead(YM3812, a) | 0x06 ;
}
int ym3812_timer_over(void *chip, int c)
{
	FM_OPL *YM3812 = (FM_OPL *)chip;
	return OPLTimerOver(YM3812, c);
}

void ym3812_set_timer_handler(void *chip, OPL_TIMERHANDLER timer_handler, device_t *device)
{
	FM_OPL *YM3812 = (FM_OPL *)chip;
	OPLSetTimerHandler(YM3812, timer_handler, device);
}
void ym3812_set_irq_handler(void *chip,OPL_IRQHANDLER IRQHandler,device_t *device)
{
	FM_OPL *YM3812 = (FM_OPL *)chip;
	OPLSetIRQHandler(YM3812, IRQHandler, device);
}
void ym3812_set_update_handler(void *chip,OPL_UPDATEHANDLER UpdateHandler,device_t *device)
{
	FM_OPL *YM3812 = (FM_OPL *)chip;
	OPLSetUpdateHandler(YM3812, UpdateHandler, device);
}

/*
** Generate samples for one of the YM3812's
**
** 'which' is the virtual YM3812 number
** '*buffer' is the output buffer pointer
** 'length' is the number of samples that should be generated
*/
void ym3812_update_one(void *chip, OPLSAMPLE *buffer, int length)
{
	FM_OPL      *OPL = (FM_OPL *)chip;
	uint8_t       rhythm = OPL->rhythm&0x20;
	OPLSAMPLE   *buf = buffer;
	int i;

	for( i=0; i < length ; i++ )
	{
		int lt;

		OPL->output[0] = 0;

		OPL->advance_lfo();

		/* FM part */
		OPL->CALC_CH(OPL->P_CH[0]);
		OPL->CALC_CH(OPL->P_CH[1]);
		OPL->CALC_CH(OPL->P_CH[2]);
		OPL->CALC_CH(OPL->P_CH[3]);
		OPL->CALC_CH(OPL->P_CH[4]);
		OPL->CALC_CH(OPL->P_CH[5]);

		if(!rhythm)
		{
			OPL->CALC_CH(OPL->P_CH[6]);
			OPL->CALC_CH(OPL->P_CH[7]);
			OPL->CALC_CH(OPL->P_CH[8]);
		}
		else        /* Rhythm part */
		{
			OPL->CALC_RH();
		}

		lt = OPL->output[0];

		lt >>= FINAL_SH;

		/* limit check */
		lt = limit( lt , MAXOUT, MINOUT );

		#ifdef SAVE_SAMPLE
		if (which==0)
		{
			SAVE_ALL_CHANNELS
		}
		#endif

		/* store to sound buffer */
		buf[i] = lt;

		OPL->advance();
	}

}

// save state support
void SaveState_Channel(OPL_CH *CH, std::ostream& stream) {
	int slot, ch;

	for( ch=0 ; ch < 9 ; ch++, CH++ ) {
		/* channel */
		WRITE_POD( &CH->block_fnum, CH->block_fnum );
	    WRITE_POD( &CH->kcode, CH->kcode );
	    /* slots */
  		for( slot=0 ; slot < 2 ; slot++ ) {
        	OPL_SLOT *SLOT = &CH->SLOT[slot];

        	WRITE_POD( &SLOT->ar, SLOT->ar );
        	WRITE_POD( &SLOT->dr, SLOT->dr );
        	WRITE_POD( &SLOT->rr, SLOT->rr );
        	WRITE_POD( &SLOT->KSR, SLOT->KSR );
       	    WRITE_POD( &SLOT->ksl, SLOT->ksl );
        	WRITE_POD( &SLOT->mul, SLOT->mul );

        	WRITE_POD( &SLOT->Cnt, SLOT->Cnt );
        	WRITE_POD( &SLOT->FB, SLOT->FB );
        	WRITE_POD( &SLOT->op1_out, SLOT->op1_out );
        	WRITE_POD( &SLOT->CON, SLOT->CON );

        	WRITE_POD( &SLOT->eg_type, SLOT->eg_type );
            WRITE_POD( &SLOT->state, SLOT->state );
            WRITE_POD( &SLOT->TL, SLOT->TL );
            WRITE_POD( &SLOT->volume, SLOT->volume );
            WRITE_POD( &SLOT->sl, SLOT->sl );
            WRITE_POD( &SLOT->key, SLOT->key );

            WRITE_POD( &SLOT->AMmask, SLOT->AMmask );
            WRITE_POD( &SLOT->vib, SLOT->vib );

            WRITE_POD( &SLOT->wavetable, SLOT->wavetable );
    	}
	}
}

void LoadState_Channel(OPL_CH *CH, std::istream& stream) {
	int slot, ch;

	for( ch=0 ; ch < 9 ; ch++, CH++ ) {
		/* channel */
	    READ_POD( &CH->block_fnum, CH->block_fnum );
	    READ_POD( &CH->kcode, CH->kcode );
	    /* slots */
   		for( slot=0 ; slot < 2 ; slot++ ) {
        	OPL_SLOT *SLOT = &CH->SLOT[slot];

        	READ_POD( &SLOT->ar, SLOT->ar );
        	READ_POD( &SLOT->dr, SLOT->dr );
        	READ_POD( &SLOT->rr, SLOT->rr );
        	READ_POD( &SLOT->KSR, SLOT->KSR );
        	READ_POD( &SLOT->ksl, SLOT->ksl );
        	READ_POD( &SLOT->mul, SLOT->mul );

        	READ_POD( &SLOT->Cnt, SLOT->Cnt );
        	READ_POD( &SLOT->FB, SLOT->FB );
        	READ_POD( &SLOT->op1_out, SLOT->op1_out );
        	READ_POD( &SLOT->CON, SLOT->CON );

        	READ_POD( &SLOT->eg_type, SLOT->eg_type );
            READ_POD( &SLOT->state, SLOT->state );
            READ_POD( &SLOT->TL, SLOT->TL );
            READ_POD( &SLOT->volume, SLOT->volume );
            READ_POD( &SLOT->sl, SLOT->sl );
            READ_POD( &SLOT->key, SLOT->key );

            READ_POD( &SLOT->AMmask, SLOT->AMmask );
            READ_POD( &SLOT->vib, SLOT->vib );

            READ_POD( &SLOT->wavetable, SLOT->wavetable );
    	}
	}
}

void FMOPL_SaveState(void *chip, std::ostream& stream ) {
    FM_OPL *OPL = (FM_OPL *)chip;

    SaveState_Channel(OPL->P_CH, stream);

    WRITE_POD(&OPL->eg_cnt, OPL->eg_cnt);
    WRITE_POD(&OPL->eg_timer, OPL->eg_timer);
    WRITE_POD(&OPL->rhythm, OPL->rhythm);

    WRITE_POD(&OPL->lfo_am_depth, OPL->lfo_am_depth);
    WRITE_POD(&OPL->lfo_pm_depth_range, OPL->lfo_pm_depth_range);
    WRITE_POD(&OPL->lfo_am_cnt, OPL->lfo_am_cnt);
    WRITE_POD(&OPL->lfo_pm_cnt, OPL->lfo_pm_cnt);

    WRITE_POD(&OPL->noise_rng, OPL->noise_rng);
    WRITE_POD(&OPL->noise_p, OPL->noise_p);

   	if( OPL->type & OPL_TYPE_WAVESEL ) {
    	WRITE_POD(&OPL->wavesel, OPL->wavesel);
  	}

    WRITE_POD(&OPL->T, OPL->T);
    WRITE_POD(&OPL->st, OPL->st);

#if BUILD_Y8950
    //ToDo - Bruenor
	/*if ( (OPL->type & OPL_TYPE_ADPCM) && (OPL->deltat) ) {
		// OPL->deltat->savestate(device);
	}

	if ( OPL->type & OPL_TYPE_IO ) {
		// device->save_item(NAME(OPL->portDirection));
		// device->save_item(NAME(OPL->portLatch));
	}*/
#endif

    WRITE_POD(&OPL->address, OPL->address);
    WRITE_POD(&OPL->status, OPL->status);
    WRITE_POD(&OPL->statusmask, OPL->statusmask);
    WRITE_POD(&OPL->mode, OPL->mode);
}

void FMOPL_LoadState(void *chip, std::istream& stream ) {
    FM_OPL *OPL = (FM_OPL *)chip;

    LoadState_Channel(OPL->P_CH, stream);

    READ_POD(&OPL->eg_cnt, OPL->eg_cnt);
    READ_POD(&OPL->eg_timer, OPL->eg_timer);

    READ_POD(&OPL->rhythm, OPL->rhythm);

    READ_POD(&OPL->lfo_am_depth, OPL->lfo_am_depth);
    READ_POD(&OPL->lfo_pm_depth_range, OPL->lfo_pm_depth_range);
    READ_POD(&OPL->lfo_am_cnt, OPL->lfo_am_cnt);
    READ_POD(&OPL->lfo_pm_cnt, OPL->lfo_pm_cnt);

    READ_POD(&OPL->noise_rng, OPL->noise_rng);
    READ_POD(&OPL->noise_p, OPL->noise_p);

   	if( OPL->type & OPL_TYPE_WAVESEL ) {
    	READ_POD(&OPL->wavesel, OPL->wavesel);
  	}

    READ_POD(&OPL->T, OPL->T);
    READ_POD(&OPL->st, OPL->st);

#if BUILD_Y8950
    //ToDo - Bruenor
#endif

    READ_POD(&OPL->address, OPL->address);
    READ_POD(&OPL->status, OPL->status);
    READ_POD(&OPL->statusmask, OPL->statusmask);
    READ_POD(&OPL->mode, OPL->mode);
}
#endif /* BUILD_YM3812 */



#if (BUILD_YM3526)

void ym3526_clock_changed(void *chip, uint32_t clock, uint32_t rate)
{
	OPL_clock_changed((FM_OPL *)chip, clock, rate);
}

void *ym3526_init(device_t *device, uint32_t clock, uint32_t rate)
{
	/* emulator create */
	FM_OPL *YM3526 = OPLCreate(device,clock,rate,OPL_TYPE_YM3526);
	if (YM3526)
	{
		//OPL_save_state(YM3526, device);
		ym3526_reset_chip(YM3526);
	}
	return YM3526;
}

void ym3526_shutdown(void *chip)
{
	FM_OPL *YM3526 = (FM_OPL *)chip;
	/* emulator shutdown */
	OPLDestroy(YM3526);
}
void ym3526_reset_chip(void *chip)
{
	FM_OPL *YM3526 = (FM_OPL *)chip;
	YM3526->ResetChip();
}

int ym3526_write(void *chip, int a, int v)
{
	FM_OPL *YM3526 = (FM_OPL *)chip;
	return OPLWrite(YM3526, a, v);
}

unsigned char ym3526_read(void *chip, int a)
{
	FM_OPL *YM3526 = (FM_OPL *)chip;
	/* YM3526 always returns bit2 and bit1 in HIGH state */
	return OPLRead(YM3526, a) | 0x06 ;
}
int ym3526_timer_over(void *chip, int c)
{
	FM_OPL *YM3526 = (FM_OPL *)chip;
	return OPLTimerOver(YM3526, c);
}

void ym3526_set_timer_handler(void *chip, OPL_TIMERHANDLER timer_handler, device_t *device)
{
	FM_OPL *YM3526 = (FM_OPL *)chip;
	OPLSetTimerHandler(YM3526, timer_handler, device);
}
void ym3526_set_irq_handler(void *chip,OPL_IRQHANDLER IRQHandler,device_t *device)
{
	FM_OPL *YM3526 = (FM_OPL *)chip;
	OPLSetIRQHandler(YM3526, IRQHandler, device);
}
void ym3526_set_update_handler(void *chip,OPL_UPDATEHANDLER UpdateHandler,device_t *device)
{
	FM_OPL *YM3526 = (FM_OPL *)chip;
	OPLSetUpdateHandler(YM3526, UpdateHandler, device);
}


/*
** Generate samples for one of the YM3526's
**
** 'which' is the virtual YM3526 number
** '*buffer' is the output buffer pointer
** 'length' is the number of samples that should be generated
*/
void ym3526_update_one(void *chip, OPLSAMPLE *buffer, int length)
{
	FM_OPL      *OPL = (FM_OPL *)chip;
	uint8_t       rhythm = OPL->rhythm&0x20;
	OPLSAMPLE   *buf = buffer;
	int i;

	for( i=0; i < length ; i++ )
	{
		int lt;

		OPL->output[0] = 0;

		OPL->advance_lfo();

		/* FM part */
		OPL->CALC_CH(OPL->P_CH[0]);
		OPL->CALC_CH(OPL->P_CH[1]);
		OPL->CALC_CH(OPL->P_CH[2]);
		OPL->CALC_CH(OPL->P_CH[3]);
		OPL->CALC_CH(OPL->P_CH[4]);
		OPL->CALC_CH(OPL->P_CH[5]);

		if(!rhythm)
		{
			OPL->CALC_CH(OPL->P_CH[6]);
			OPL->CALC_CH(OPL->P_CH[7]);
			OPL->CALC_CH(OPL->P_CH[8]);
		}
		else        /* Rhythm part */
		{
			OPL->CALC_RH();
		}

		lt = OPL->output[0];

		lt >>= FINAL_SH;

		/* limit check */
		lt = limit( lt , MAXOUT, MINOUT );

		#ifdef SAVE_SAMPLE
		if (which==0)
		{
			SAVE_ALL_CHANNELS
		}
		#endif

		/* store to sound buffer */
		buf[i] = lt;

		OPL->advance();
	}

}
#endif /* BUILD_YM3526 */




#if BUILD_Y8950

static void Y8950_deltat_status_set(void *chip, uint8_t changebits)
{
	FM_OPL *Y8950 = (FM_OPL *)chip;
	Y8950->STATUS_SET(changebits);
}
static void Y8950_deltat_status_reset(void *chip, uint8_t changebits)
{
	FM_OPL *Y8950 = (FM_OPL *)chip;
	Y8950->STATUS_RESET(changebits);
}

void *y8950_init(device_t *device, uint32_t clock, uint32_t rate)
{
	/* emulator create */
	FM_OPL *Y8950 = OPLCreate(device,clock,rate,OPL_TYPE_Y8950);
	if (Y8950)
	{
		Y8950->deltat->status_set_handler = Y8950_deltat_status_set;
		Y8950->deltat->status_reset_handler = Y8950_deltat_status_reset;
		Y8950->deltat->status_change_which_chip = Y8950;
		Y8950->deltat->status_change_EOS_bit = 0x10;        /* status flag: set bit4 on End Of Sample */
		Y8950->deltat->status_change_BRDY_bit = 0x08;   /* status flag: set bit3 on BRDY (End Of: ADPCM analysis/synthesis, memory reading/writing) */

		/*Y8950->deltat->write_time = 10.0 / clock;*/       /* a single byte write takes 10 cycles of main clock */
		/*Y8950->deltat->read_time  = 8.0 / clock;*/        /* a single byte read takes 8 cycles of main clock */
		/* reset */
		//OPL_save_state(Y8950, device);
		y8950_reset_chip(Y8950);
	}

	return Y8950;
}

void y8950_shutdown(void *chip)
{
	FM_OPL *Y8950 = (FM_OPL *)chip;
	/* emulator shutdown */
	OPLDestroy(Y8950);
}
void y8950_reset_chip(void *chip)
{
	FM_OPL *Y8950 = (FM_OPL *)chip;
	Y8950->ResetChip();
}

int y8950_write(void *chip, int a, int v)
{
	FM_OPL *Y8950 = (FM_OPL *)chip;
	return OPLWrite(Y8950, a, v);
}

unsigned char y8950_read(void *chip, int a)
{
	FM_OPL *Y8950 = (FM_OPL *)chip;
	return OPLRead(Y8950, a);
}
int y8950_timer_over(void *chip, int c)
{
	FM_OPL *Y8950 = (FM_OPL *)chip;
	return OPLTimerOver(Y8950, c);
}

void y8950_set_timer_handler(void *chip, OPL_TIMERHANDLER timer_handler, device_t *device)
{
	FM_OPL *Y8950 = (FM_OPL *)chip;
	OPLSetTimerHandler(Y8950, timer_handler, device);
}
void y8950_set_irq_handler(void *chip,OPL_IRQHANDLER IRQHandler,device_t *device)
{
	FM_OPL *Y8950 = (FM_OPL *)chip;
	OPLSetIRQHandler(Y8950, IRQHandler, device);
}
void y8950_set_update_handler(void *chip,OPL_UPDATEHANDLER UpdateHandler,device_t *device)
{
	FM_OPL *Y8950 = (FM_OPL *)chip;
	OPLSetUpdateHandler(Y8950, UpdateHandler, device);
}

void y8950_set_delta_t_memory(void *chip, void * deltat_mem_ptr, int deltat_mem_size )
{
	FM_OPL      *OPL = (FM_OPL *)chip;
	OPL->deltat->memory = (uint8_t *)(deltat_mem_ptr);
	OPL->deltat->memory_size = deltat_mem_size;
}

/*
** Generate samples for one of the Y8950's
**
** 'which' is the virtual Y8950 number
** '*buffer' is the output buffer pointer
** 'length' is the number of samples that should be generated
*/
void y8950_update_one(void *chip, OPLSAMPLE *buffer, int length)
{
	int i;
	FM_OPL      *OPL = (FM_OPL *)chip;
	uint8_t       rhythm  = OPL->rhythm&0x20;
	YM_DELTAT   *DELTAT = OPL->deltat;
	OPLSAMPLE   *buf    = buffer;

	for( i=0; i < length ; i++ )
	{
		int lt;

		OPL->output[0] = 0;
		OPL->output_deltat[0] = 0;

		OPL->advance_lfo();

		/* deltaT ADPCM */
		if( DELTAT->portstate&0x80 )
			DELTAT->ADPCM_CALC();

		/* FM part */
		OPL->CALC_CH(OPL->P_CH[0]);
		OPL->CALC_CH(OPL->P_CH[1]);
		OPL->CALC_CH(OPL->P_CH[2]);
		OPL->CALC_CH(OPL->P_CH[3]);
		OPL->CALC_CH(OPL->P_CH[4]);
		OPL->CALC_CH(OPL->P_CH[5]);

		if(!rhythm)
		{
			OPL->CALC_CH(OPL->P_CH[6]);
			OPL->CALC_CH(OPL->P_CH[7]);
			OPL->CALC_CH(OPL->P_CH[8]);
		}
		else        /* Rhythm part */
		{
			OPL->CALC_RH();
		}

		lt = OPL->output[0] + (OPL->output_deltat[0]>>11);

		lt >>= FINAL_SH;

		/* limit check */
		lt = limit( lt , MAXOUT, MINOUT );

		#ifdef SAVE_SAMPLE
		if (which==0)
		{
			SAVE_ALL_CHANNELS
		}
		#endif

		/* store to sound buffer */
		buf[i] = lt;

		OPL->advance();
	}

}

void y8950_set_port_handler(void *chip,OPL_PORTHANDLER_W PortHandler_w,OPL_PORTHANDLER_R PortHandler_r,device_t *device)
{
	FM_OPL      *OPL = (FM_OPL *)chip;
	OPL->porthandler_w = PortHandler_w;
	OPL->porthandler_r = PortHandler_r;
	OPL->port_param = device;
}

void y8950_set_keyboard_handler(void *chip,OPL_PORTHANDLER_W KeyboardHandler_w,OPL_PORTHANDLER_R KeyboardHandler_r,device_t *device)
{
	FM_OPL      *OPL = (FM_OPL *)chip;
	OPL->keyboardhandler_w = KeyboardHandler_w;
	OPL->keyboardhandler_r = KeyboardHandler_r;
	OPL->keyboard_param = device;
}


#endif
