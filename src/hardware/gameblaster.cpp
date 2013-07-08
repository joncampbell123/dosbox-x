/*
 *  Copyright (C) 2002-2010  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "mem.h"
#include "hardware.h"
#include "setup.h"
#include "support.h"
#include "pic.h"
#include <cstring>
#include <math.h>


#define LEFT	0x00
#define RIGHT	0x01
#define CMS_BUFFER_SIZE 128
#define CMS_RATE 22050


typedef Bit8u UINT8;
typedef Bit16s INT16;

/* this structure defines a channel */
struct saa1099_channel
{
	int frequency;			/* frequency (0x00..0xff) */
	int freq_enable;		/* frequency enable */
	int noise_enable;		/* noise enable */
	int octave; 			/* octave (0x00..0x07) */
	int amplitude[2];		/* amplitude (0x00..0x0f) */
	int envelope[2];		/* envelope (0x00..0x0f or 0x10 == off) */

	/* vars to simulate the square wave */
	double counter;
	double freq;
	int level;
};

/* this structure defines a noise channel */
struct saa1099_noise
{
	/* vars to simulate the noise generator output */
	double counter;
	double freq;
	int level;						/* noise polynomal shifter */
};

/* this structure defines a SAA1099 chip */
struct SAA1099
{
	int stream;						/* our stream */
	int noise_params[2];			/* noise generators parameters */
	int env_enable[2];				/* envelope generators enable */
	int env_reverse_right[2];		/* envelope reversed for right channel */
	int env_mode[2];				/* envelope generators mode */
	int env_bits[2];				/* non zero = 3 bits resolution */
	int env_clock[2];				/* envelope clock mode (non-zero external) */
    int env_step[2];                /* current envelope step */
	int all_ch_enable;				/* all channels enable */
	int sync_state;					/* sync all channels */
	int selected_reg;				/* selected register */
	struct saa1099_channel channels[6];    /* channels */
	struct saa1099_noise noise[2];	/* noise generators */
};

static const UINT8 envelope[8][64] = {
	/* zero amplitude */
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* maximum amplitude */
    {15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
	 15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
	 15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
     15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15, },
	/* single decay */
	{15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* repetitive decay */
	{15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
	 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
	 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
	 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 },
	/* single triangular */
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
	 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* repetitive triangular */
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
	 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
	  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
	 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 },
	/* single attack */
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* repetitive attack */
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
	  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
	  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15 }
};


static const int amplitude_lookup[16] = {
	 0*32767/16,  1*32767/16,  2*32767/16,	3*32767/16,
	 4*32767/16,  5*32767/16,  6*32767/16,	7*32767/16,
	 8*32767/16,  9*32767/16, 10*32767/16, 11*32767/16,
	12*32767/16, 13*32767/16, 14*32767/16, 15*32767/16
};

/* global parameters */
static double sample_rate;
static SAA1099 saa1099[2];
static MixerChannel * cms_chan;
static Bit16s cms_buffer[2][2][CMS_BUFFER_SIZE];
static Bit16s * cms_buf_point[4] = {
	cms_buffer[0][0],cms_buffer[0][1],cms_buffer[1][0],cms_buffer[1][1] };

static Bitu last_command;
static Bitu base_port;


static void saa1099_envelope(int chip, int ch)
{
	struct SAA1099 *saa = &saa1099[chip];
	if (saa->env_enable[ch])
	{
		int step, mode, mask;
        mode = saa->env_mode[ch];
		/* step from 0..63 and then loop in steps 32..63 */
		step = saa->env_step[ch] =
			((saa->env_step[ch] + 1) & 0x3f) | (saa->env_step[ch] & 0x20);

		mask = 15;
        if (saa->env_bits[ch])
			mask &= ~1; 	/* 3 bit resolution, mask LSB */

        saa->channels[ch*3+0].envelope[ LEFT] =
		saa->channels[ch*3+1].envelope[ LEFT] =
		saa->channels[ch*3+2].envelope[ LEFT] = envelope[mode][step] & mask;
		if (saa->env_reverse_right[ch] & 0x01)
		{
			saa->channels[ch*3+0].envelope[RIGHT] =
			saa->channels[ch*3+1].envelope[RIGHT] =
			saa->channels[ch*3+2].envelope[RIGHT] = (15 - envelope[mode][step]) & mask;
		}
		else
		{
			saa->channels[ch*3+0].envelope[RIGHT] =
			saa->channels[ch*3+1].envelope[RIGHT] =
			saa->channels[ch*3+2].envelope[RIGHT] = envelope[mode][step] & mask;
        }
	}
	else
	{
		/* envelope mode off, set all envelope factors to 16 */
		saa->channels[ch*3+0].envelope[ LEFT] =
		saa->channels[ch*3+1].envelope[ LEFT] =
		saa->channels[ch*3+2].envelope[ LEFT] =
		saa->channels[ch*3+0].envelope[RIGHT] =
		saa->channels[ch*3+1].envelope[RIGHT] =
		saa->channels[ch*3+2].envelope[RIGHT] = 16;
    }
}


static void saa1099_update(int chip, INT16 **buffer, int length)
{
	struct SAA1099 *saa = &saa1099[chip];
    int j, ch;

	/* if the channels are disabled we're done */
	if (!saa->all_ch_enable)
	{
		/* init output data */
		memset(buffer[LEFT],0,length*sizeof(INT16));
		memset(buffer[RIGHT],0,length*sizeof(INT16));
        return;
	}

    for (ch = 0; ch < 2; ch++)
    {
		switch (saa->noise_params[ch])
		{
		case 0: saa->noise[ch].freq = 31250.0 * 2; break;
		case 1: saa->noise[ch].freq = 15625.0 * 2; break;
		case 2: saa->noise[ch].freq =  7812.5 * 2; break;
		case 3: saa->noise[ch].freq = saa->channels[ch * 3].freq; break;
		}
	}

    /* fill all data needed */
	for( j = 0; j < length; j++ )
	{
		int output_l = 0, output_r = 0;

		/* for each channel */
		for (ch = 0; ch < 6; ch++)
		{
            if (saa->channels[ch].freq == 0.0)
                saa->channels[ch].freq = (double)((2 * 15625) << saa->channels[ch].octave) /
                    (511.0 - (double)saa->channels[ch].frequency);

            /* check the actual position in the square wave */
            saa->channels[ch].counter -= saa->channels[ch].freq;
			while (saa->channels[ch].counter < 0)
			{
				/* calculate new frequency now after the half wave is updated */
				saa->channels[ch].freq = (double)((2 * 15625) << saa->channels[ch].octave) /
					(511.0 - (double)saa->channels[ch].frequency);

				saa->channels[ch].counter += sample_rate;
				saa->channels[ch].level ^= 1;

				/* eventually clock the envelope counters */
				if (ch == 1 && saa->env_clock[0] == 0)
					saa1099_envelope(chip, 0);
				if (ch == 4 && saa->env_clock[1] == 0)
					saa1099_envelope(chip, 1);
			}

			/* if the noise is enabled */
			if (saa->channels[ch].noise_enable)
			{
				/* if the noise level is high (noise 0: chan 0-2, noise 1: chan 3-5) */
				if (saa->noise[ch/3].level & 1)
				{
					/* subtract to avoid overflows, also use only half amplitude */
					output_l -= saa->channels[ch].amplitude[ LEFT] * saa->channels[ch].envelope[ LEFT] / 16 / 2;
					output_r -= saa->channels[ch].amplitude[RIGHT] * saa->channels[ch].envelope[RIGHT] / 16 / 2;
				}
			}

			/* if the square wave is enabled */
			if (saa->channels[ch].freq_enable)
			{
				/* if the channel level is high */
				if (saa->channels[ch].level & 1)
				{
					output_l += saa->channels[ch].amplitude[ LEFT] * saa->channels[ch].envelope[ LEFT] / 16;
					output_r += saa->channels[ch].amplitude[RIGHT] * saa->channels[ch].envelope[RIGHT] / 16;
				}
			}
		}

		for (ch = 0; ch < 2; ch++)
		{
			/* check the actual position in noise generator */
			saa->noise[ch].counter -= saa->noise[ch].freq;
			while (saa->noise[ch].counter < 0)
			{
				saa->noise[ch].counter += sample_rate;
				if( ((saa->noise[ch].level & 0x4000) == 0) == ((saa->noise[ch].level & 0x0040) == 0) )
					saa->noise[ch].level = (saa->noise[ch].level << 1) | 1;
				else
					saa->noise[ch].level <<= 1;
			}
		}
        /* write sound data to the buffer */
		buffer[LEFT][j] = output_l / 6;
		buffer[RIGHT][j] = output_r / 6;
	}
}

static void saa1099_write_port_w( int chip, int offset, int data )
{
	struct SAA1099 *saa = &saa1099[chip];
	if(offset == 1) {
		// address port
		saa->selected_reg = data & 0x1f;
		if (saa->selected_reg == 0x18 || saa->selected_reg == 0x19) {
			/* clock the envelope channels */
			if (saa->env_clock[0]) saa1099_envelope(chip,0);
			if (saa->env_clock[1]) saa1099_envelope(chip,1);
		}
		return;
	}
	int reg = saa->selected_reg;
	int ch;

	switch (reg)
	{
	/* channel i amplitude */
	case 0x00:	case 0x01:	case 0x02:	case 0x03:	case 0x04:	case 0x05:
		ch = reg & 7;
		saa->channels[ch].amplitude[LEFT] = amplitude_lookup[data & 0x0f];
		saa->channels[ch].amplitude[RIGHT] = amplitude_lookup[(data >> 4) & 0x0f];
		break;
	/* channel i frequency */
	case 0x08:	case 0x09:	case 0x0a:	case 0x0b:	case 0x0c:	case 0x0d:
		ch = reg & 7;
		saa->channels[ch].frequency = data & 0xff;
		break;
	/* channel i octave */
	case 0x10:	case 0x11:	case 0x12:
		ch = (reg - 0x10) << 1;
		saa->channels[ch + 0].octave = data & 0x07;
		saa->channels[ch + 1].octave = (data >> 4) & 0x07;
		break;
	/* channel i frequency enable */
	case 0x14:
		saa->channels[0].freq_enable = data & 0x01;
		saa->channels[1].freq_enable = data & 0x02;
		saa->channels[2].freq_enable = data & 0x04;
		saa->channels[3].freq_enable = data & 0x08;
		saa->channels[4].freq_enable = data & 0x10;
		saa->channels[5].freq_enable = data & 0x20;
		break;
	/* channel i noise enable */
	case 0x15:
		saa->channels[0].noise_enable = data & 0x01;
		saa->channels[1].noise_enable = data & 0x02;
		saa->channels[2].noise_enable = data & 0x04;
		saa->channels[3].noise_enable = data & 0x08;
		saa->channels[4].noise_enable = data & 0x10;
		saa->channels[5].noise_enable = data & 0x20;
		break;
	/* noise generators parameters */
	case 0x16:
		saa->noise_params[0] = data & 0x03;
		saa->noise_params[1] = (data >> 4) & 0x03;
		break;
	/* envelope generators parameters */
	case 0x18:	case 0x19:
		ch = reg - 0x18;
		saa->env_reverse_right[ch] = data & 0x01;
		saa->env_mode[ch] = (data >> 1) & 0x07;
		saa->env_bits[ch] = data & 0x10;
		saa->env_clock[ch] = data & 0x20;
		saa->env_enable[ch] = data & 0x80;
		/* reset the envelope */
		saa->env_step[ch] = 0;
		break;
	/* channels enable & reset generators */
	case 0x1c:
		saa->all_ch_enable = data & 0x01;
		saa->sync_state = data & 0x02;
		if (data & 0x02)
		{
			int i;
//			logerror("%04x: (SAA1099 #%d) -reg 0x1c- Chip reset\n",activecpu_get_pc(), chip);
			/* Synch & Reset generators */
			for (i = 0; i < 6; i++)
			{
                saa->channels[i].level = 0;
				saa->channels[i].counter = 0.0;
			}
		}
		break;
	default:	/* Error! */
//		logerror("%04x: (SAA1099 #%d) Unknown operation (reg:%02x, data:%02x)\n",activecpu_get_pc(), chip, reg, data);
		LOG(LOG_MISC,LOG_ERROR)("CMS Unkown write to reg %x with %x",reg, data);
	}
}

static void write_cms(Bitu port, Bitu val, Bitu /* iolen */) {
	if(cms_chan && (!cms_chan->enabled)) cms_chan->Enable(true);
	last_command = PIC_Ticks;
	switch (port-base_port) {
	case 0:
		saa1099_write_port_w(0,0,val);
		break;
	case 1:
		saa1099_write_port_w(0,1,val);
		break;
	case 2:
		saa1099_write_port_w(1,0,val);
		break;
	case 3:
		saa1099_write_port_w(1,1,val);
		break;
	}
}

static void CMS_CallBack(Bitu len) {
	if (len > CMS_BUFFER_SIZE) return;

	saa1099_update(0, &cms_buf_point[0], (int)len);
	saa1099_update(1, &cms_buf_point[2], (int)len);

	 Bit16s * stream=(Bit16s *) MixTemp;
	/* Mix chip outputs */
	for (Bitu l=0;l<len;l++) {
		register Bits left, right;
		left = cms_buffer[0][LEFT][l] + cms_buffer[1][LEFT][l];
		right = cms_buffer[0][RIGHT][l] + cms_buffer[1][RIGHT][l];

		if (left>MAX_AUDIO) *stream=MAX_AUDIO;
		else if (left<MIN_AUDIO) *stream=MIN_AUDIO;
		else *stream=(Bit16s)left;
		stream++;

		if (right>MAX_AUDIO) *stream=MAX_AUDIO;
		else if (right<MIN_AUDIO) *stream=MIN_AUDIO;
		else *stream=(Bit16s)right;
		stream++;
	}
	if(cms_chan) cms_chan->AddSamples_s16(len,(Bit16s *)MixTemp);
	if (last_command + 10000 < PIC_Ticks) if(cms_chan) cms_chan->Enable(false);
}

// The Gameblaster detection
static Bit8u cms_detect_register = 0xff;

static void write_cms_detect(Bitu port, Bitu val, Bitu /* iolen */) {
	switch(port-base_port) {
	case 0x6:
	case 0x7:
		cms_detect_register = val;
		break;
	}
}

static Bitu read_cms_detect(Bitu port, Bitu /* iolen */) {
	Bit8u retval = 0xff;
	switch(port-base_port) {
	case 0x4:
		retval = 0x7f;
		break;
	case 0xa:
	case 0xb:
		retval = cms_detect_register;
		break;
	}
	return retval;
}


class CMS:public Module_base {
private:
	IO_WriteHandleObject WriteHandler;
	IO_WriteHandleObject DetWriteHandler;
	IO_ReadHandleObject DetReadHandler;
	MixerObject MixerChan;

public:
	CMS(Section* configuration):Module_base(configuration) {
		Section_prop * section = static_cast<Section_prop *>(configuration);
		Bitu sample_rate_temp = section->Get_int("oplrate");
		sample_rate = static_cast<double>(sample_rate_temp);
		base_port = section->Get_hex("sbbase");
		WriteHandler.Install(base_port, write_cms, IO_MB,4);

		// A standalone Gameblaster has a magic chip on it which is
		// sometimes used for detection.
		const char * sbtype=section->Get_string("sbtype");
		if (!strcasecmp(sbtype,"gb")) {
			DetWriteHandler.Install(base_port+4,write_cms_detect,IO_MB,12);
			DetReadHandler.Install(base_port,read_cms_detect,IO_MB,16);
		}

		/* Register the Mixer CallBack */
		cms_chan = MixerChan.Install(CMS_CallBack,sample_rate_temp,"CMS");
	
		last_command = PIC_Ticks;
	
		for (int s=0;s<2;s++) {
			struct SAA1099 *saa = &saa1099[s];
			memset(saa, 0, sizeof(struct SAA1099));
		}
	}
	~CMS() {
		cms_chan = 0;
	}
};


static CMS* test;
   
void CMS_Init(Section* sec) {
	test = new CMS(sec);
}
void CMS_ShutDown(Section* sec) {
	delete test;	       
}
