/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
/* FIXME: This code... well... it could be done better.
 *        It was written back when all mixer callbacks were potentially done from
 *        the SDL audio thread. DOSBox-X has since moved to a model where within
 *        the timer tick, audio can be rendered up to the "current time" at any
 *        time. So while the event queue idea was appropriate at the time, it's
 *        no longer needed and just like the GUS and SB code could be written to
 *        trigger a mixer render "up-to" for every change instead. --J.C. */
 
//#define SPKR_DEBUGGING
#include <math.h>
#include "dosbox.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "pic.h"
#include "control.h"

#ifdef SPKR_DEBUGGING
FILE* PCSpeakerLog = NULL;
FILE* PCSpeakerState = NULL;
FILE* PCSpeakerOutput = NULL;
FILE* PCSpeakerOutputLevelLog = NULL;

typedef enum {
	LABEL_CONTROL,
	LABEL_COUNTER,
	LABEL_OUTPUT
} state_label_e;

typedef struct {
	bool pit_output_enabled;
	bool pit_clock_gate_enabled;
} output_state_t;

typedef union {
	unsigned char pit_mode;
	uint32_t counter;
	output_state_t output_state;
} state_specific_data_u;

typedef struct {
	double timestamp;
	state_label_e state_label;
	state_specific_data_u state_specific_data;
} speaker_state_change_t;

#endif

#define SPKR_ENTRIES 8192
#define SPKR_VOLUME 10000
//#define SPKR_SHIFT 8
pic_tickindex_t SPKR_SPEED = 1.0;

struct DelayEntry {
	pic_tickindex_t index;
	bool output_level;
};

static struct {
	MixerChannel * chan;
	Bitu pit_mode;
	Bitu rate;

	bool  pit_output_enabled;
	bool  pit_clock_gate_enabled;
	bool  pit_output_level;
	pic_tickindex_t pit_new_max,pit_new_half;
	pic_tickindex_t pit_max,pit_half;
	pic_tickindex_t pit_index;
	bool  pit_mode1_waiting_for_counter;
	bool  pit_mode1_waiting_for_trigger;
	pic_tickindex_t pit_mode1_pending_max;

	bool  pit_mode3_counting;
	pic_tickindex_t volwant,volcur;
	Bitu last_ticks;
	pic_tickindex_t last_index;
	Bitu minimum_counter;
	DelayEntry entries[SPKR_ENTRIES];
	Bitu used;
} spkr;

inline static void AddDelayEntry(pic_tickindex_t index, bool new_output_level) {
#ifdef SPKR_DEBUGGING
	if (index < 0 || index > 1) {
		LOG_MSG("AddDelayEntry: index out of range %f at %f", index, PIC_FullIndex());
	}
#endif
	static bool previous_output_level = 0;
	if (new_output_level == previous_output_level) {
		return;
	}
	previous_output_level = new_output_level;
	if (spkr.used == SPKR_ENTRIES) {
        LOG(LOG_MISC,LOG_WARN)("PC speaker delay entry queue overrun");
		return;
	}
	spkr.entries[spkr.used].index=index;
	spkr.entries[spkr.used].output_level=new_output_level;
	spkr.used++;
}

inline static void AddPITOutput(pic_tickindex_t index) {
	if (spkr.pit_output_enabled) {
		AddDelayEntry(index, spkr.pit_output_level);
	}
}

pic_tickindex_t speaker_pit_delta(void);
void speaker_pit_update(void);

static void CheckPITSynchronization(void) {
    if (spkr.pit_clock_gate_enabled) {
        speaker_pit_update();

        const pic_tickindex_t now_rel = speaker_pit_delta();
        pic_tickindex_t delta = spkr.pit_index - now_rel;

        if (spkr.pit_mode == 3) {
            if (delta >= (spkr.pit_half/2))
                delta -=  spkr.pit_half;
        }
        else {
            if (delta >=  spkr.pit_half)
                delta -=  spkr.pit_max;
        }

        // FIXME: This code is also detecting many sync errors regarding Mode 0 aka the
        //        "Realsound" (PWM) method of playing digitized speech, though ironically
        //        this bludgeon seems to vastly improve the sound quality!
        // FIXME: This code maintains good synchronization EXCEPT in the case of Mode 3
        //        with rapid changes to the counter WITHOUT writing port 43h (new_mode) first.
        //        This bludgeon is here to correct that. This is the problem with timer.cpp
        //        and pcspeaker.cpp tracking the counter separately.
        if (fabs(delta) >= (4.1 / CPU_CycleMax)) {
#if 0//enable this when debugging PC speaker code
            LOG_MSG("PIT speaker synchronization error %.9f",delta);
#endif
            spkr.pit_index = now_rel;
        }
        else {
            spkr.pit_index -= delta * 0.005;
        }
    }
}

static void ForwardPIT(pic_tickindex_t newindex) {
#ifdef SPKR_DEBUGGING
	if (newindex < 0 || newindex > 1) {
		LOG_MSG("ForwardPIT: index out of range %f at %f", newindex, PIC_FullIndex());
	}
#endif
	pic_tickindex_t passed=(newindex-spkr.last_index);
	pic_tickindex_t delay_base=spkr.last_index;
	spkr.last_index=newindex;
	switch (spkr.pit_mode) {
	case 6: // dummy
		return;
	case 0:
		if (spkr.pit_index >= spkr.pit_max) {
			return; // counter reached zero before previous call so do nothing
		}
		spkr.pit_index += passed;
		if (spkr.pit_index >= spkr.pit_max) {
			// counter reached zero between previous and this call
			pic_tickindex_t delay = delay_base;
			delay += spkr.pit_max - spkr.pit_index + passed;
			spkr.pit_output_level = 1;
			AddPITOutput(delay);
		}
		return;
	case 1:
		if (spkr.pit_mode1_waiting_for_counter) {
			// assert output level is high
			return; // counter not written yet
		}
		if (spkr.pit_mode1_waiting_for_trigger) {
			// assert output level is high
			return; // no pulse yet
		}
		if (spkr.pit_index >= spkr.pit_max) {
			return; // counter reached zero before previous call so do nothing
		}
		spkr.pit_index += passed;
		if (spkr.pit_index >= spkr.pit_max) {
			// counter reached zero between previous and this call
			pic_tickindex_t delay = delay_base;
			delay += spkr.pit_max - spkr.pit_index + passed;
			spkr.pit_output_level = 1;
			AddPITOutput(delay);
			// finished with this pulse
			spkr.pit_mode1_waiting_for_trigger = 1;
		}
		return;
	case 2:
		while (passed>0) {
			/* passed the initial low cycle? */
			if (spkr.pit_index>=spkr.pit_half) {
				/* Start a new low cycle */
				if ((spkr.pit_index+passed)>=spkr.pit_max) {
					pic_tickindex_t delay=spkr.pit_max-spkr.pit_index;
					delay_base+=delay;passed-=delay;
					spkr.pit_output_level = 0;
					AddPITOutput(delay_base);
					spkr.pit_index=0;
				} else {
					spkr.pit_index+=passed;
					return;
				}
			} else {
				if ((spkr.pit_index+passed)>=spkr.pit_half) {
					pic_tickindex_t delay=spkr.pit_half-spkr.pit_index;
					delay_base+=delay;passed-=delay;
					spkr.pit_output_level = 1;
					AddPITOutput(delay_base);
					spkr.pit_index=spkr.pit_half;
				} else {
					spkr.pit_index+=passed;
					return;
				}
			}
		}
		break;
		//END CASE 2
	case 3:
        /* this version will only count up to pit_half. pit_max is ignored */
		if (!spkr.pit_mode3_counting) break;
		while (passed>0) {
			/* Determine where in the wave we're located */
            if ((spkr.pit_index+passed)>=spkr.pit_half) {
                pic_tickindex_t delay=spkr.pit_half-spkr.pit_index;
                delay_base+=delay;passed-=delay;
                spkr.pit_output_level = !spkr.pit_output_level;
                AddPITOutput(delay_base);
                spkr.pit_index=0;
                /* Load the new count */
                spkr.pit_half=spkr.pit_new_half;
                spkr.pit_max=spkr.pit_new_max;
            } else {
                spkr.pit_index+=passed;
                return;
            }
        }
		break;
		//END CASE 3
	case 4:
		if (spkr.pit_index<spkr.pit_max) {
			/* Check if we're gonna pass the end this block */
			if (spkr.pit_index+passed>=spkr.pit_max) {
				pic_tickindex_t delay=spkr.pit_max-spkr.pit_index;
				delay_base+=delay;passed-=delay;
				spkr.pit_output_level = 0;
				AddPITOutput(delay_base); //No new events unless reprogrammed
				spkr.pit_index=spkr.pit_max;
			} else spkr.pit_index+=passed;
		}
		break;
		//END CASE 4
	}
}

void PCSPEAKER_SetPITControl(Bitu mode) {
    if (spkr.chan == NULL)
        return;

    pic_tickindex_t newindex = PIC_TickIndex();
	ForwardPIT(newindex);
#ifdef SPKR_DEBUGGING
	fprintf(PCSpeakerLog, "%f pit command: %u\n", PIC_FullIndex(), mode);
	speaker_state_change_t temp;
	memset(&temp, 0, sizeof(speaker_state_change_t));
	temp.timestamp = PIC_FullIndex();
	temp.state_label = LABEL_CONTROL;
	temp.state_specific_data.pit_mode = mode;
	if (fwrite(&temp, sizeof(temp), 1, PCSpeakerState) != 1)
		LOG_MSG(__FILE__ ": unable to write to pc speaker log");
#endif
	// TODO: implement all modes
	switch(mode) {
    case 0:
		spkr.pit_mode = 0;
        break;
	case 1:
		spkr.pit_mode = 1;
		spkr.pit_mode1_waiting_for_counter = 1;
		spkr.pit_mode1_waiting_for_trigger = 0;
		spkr.pit_output_level = 1;
		break;
	case 3:
		spkr.pit_mode = 3;
		spkr.pit_mode3_counting = 0;
		spkr.pit_output_level = 1;
		break;
	default:
		return;
	}
	AddPITOutput(newindex);
}

// new mode WITHOUT writing port 43h
void PCSPEAKER_SetCounter_NoNewMode(Bitu cntr) {
    if (spkr.chan == NULL)
        return;

    if (!spkr.last_ticks) {
		if(spkr.chan) spkr.chan->Enable(true);
		spkr.last_index=0;
	}
	spkr.last_ticks=PIC_Ticks;
	pic_tickindex_t newindex=PIC_TickIndex();
	ForwardPIT(newindex);
	switch (spkr.pit_mode) {
	case 0:		/* Mode 0 one shot, used with "realsound" (PWM) */
        //FIXME
		break;
	case 1: // retriggerable one-shot, used by Star Control 1
        //FIXME
		break;
	case 2:			/* Single cycle low, rest low high generator */
        //FIXME
		break;
	case 3:		/* Square wave generator */
		if (cntr < spkr.minimum_counter) {
//#ifdef SPKR_DEBUGGING
//			LOG_MSG(
//				"SetCounter: too high frequency %u (cntr %u) at %f",
//				PIT_TICK_RATE/cntr,
//				cntr,
//				PIC_FullIndex());
//#endif
			// hack to save CPU cycles
			cntr = spkr.minimum_counter;
			//spkr.pit_output_level = 1; // avoid breaking digger music
			//spkr.pit_mode = 6; // dummy mode with constant output
			//AddPITOutput(newindex);
			//return;
		}
		spkr.pit_new_max = (1000.0f/PIT_TICK_RATE)*cntr;
		spkr.pit_new_half=spkr.pit_new_max/2;
		if (!spkr.pit_mode3_counting) {
			spkr.pit_index = 0;
			spkr.pit_max = spkr.pit_new_max;
			spkr.pit_half = spkr.pit_new_half;
		}
		break;
	case 4:		/* Software triggered strobe */
        //FIXME
		break;
	default:
#ifdef SPKR_DEBUGGING
		LOG_MSG("Unhandled speaker mode %d at %f", mode, PIC_FullIndex());
#endif
		return;
	}
}

static bool pit_raw_clock_gate_enabled = false;

bool TIMER2_ClockGateEnabled(void);

void PCSPEAKER_UpdateType(void) {
    PCSPEAKER_SetType(pit_raw_clock_gate_enabled,spkr.pit_output_enabled);
}

void PCSPEAKER_SetCounter(Bitu cntr, Bitu mode) {
    if (spkr.chan == NULL)
        return;

#ifdef SPKR_DEBUGGING
	fprintf(PCSpeakerLog, "%f counter: %u, mode: %u\n", PIC_FullIndex(), cntr, mode);
	speaker_state_change_t temp;
	memset(&temp, 0, sizeof(speaker_state_change_t));
	temp.timestamp = PIC_FullIndex();
	temp.state_label = LABEL_COUNTER;
	temp.state_specific_data.counter = cntr;
	if (fwrite(&temp, sizeof(temp), 1, PCSpeakerState) != 1)
		LOG_MSG(__FILE__ ": unable to write to pc speaker log");
#endif
	if (!spkr.last_ticks) {
		if(spkr.chan) spkr.chan->Enable(true);
		spkr.last_index=0;
	}
	spkr.last_ticks=PIC_Ticks;
	pic_tickindex_t newindex=PIC_TickIndex();
	ForwardPIT(newindex);
    spkr.pit_index = 0; // THIS function is always called on the port 43h reset case
    switch (mode) {
	case 0:		/* Mode 0 one shot, used with "realsound" (PWM) */
		//if (cntr>80) { 
		//	cntr=80;
		//}
		//spkr.pit_output_level=((pic_tickindex_t)cntr-40)*(SPKR_VOLUME/40.0f);
		spkr.pit_output_level = 0;
		spkr.pit_max = (1000.0f / PIT_TICK_RATE) * cntr;
		AddPITOutput(newindex);
		break;
	case 1: // retriggerable one-shot, used by Star Control 1
		spkr.pit_mode1_pending_max = (1000.0f / PIT_TICK_RATE) * cntr;
		if (spkr.pit_mode1_waiting_for_counter) {
			// assert output level is high
			spkr.pit_mode1_waiting_for_counter = 0;
			spkr.pit_mode1_waiting_for_trigger = 1;
		}
		break;
	case 2:			/* Single cycle low, rest low high generator */
		spkr.pit_output_level = 0;
		AddPITOutput(newindex);
		spkr.pit_half=(1000.0f/PIT_TICK_RATE)*1;
		spkr.pit_max=(1000.0f/PIT_TICK_RATE)*cntr;
		break;
	case 3:		/* Square wave generator */
		if (cntr < spkr.minimum_counter) {
//#ifdef SPKR_DEBUGGING
//			LOG_MSG(
//				"SetCounter: too high frequency %u (cntr %u) at %f",
//				PIT_TICK_RATE/cntr,
//				cntr,
//				PIC_FullIndex());
//#endif
			// hack to save CPU cycles
			cntr = spkr.minimum_counter;
			//spkr.pit_output_level = 1; // avoid breaking digger music
			//spkr.pit_mode = 6; // dummy mode with constant output
			//AddPITOutput(newindex);
			//return;
		}
		spkr.pit_new_max = (1000.0f/PIT_TICK_RATE)*cntr;
		spkr.pit_new_half=spkr.pit_new_max/2;
		if (!spkr.pit_mode3_counting) {
			spkr.pit_max = spkr.pit_new_max;
			spkr.pit_half = spkr.pit_new_half;
			if (spkr.pit_clock_gate_enabled) {
				spkr.pit_mode3_counting = 1;
				spkr.pit_output_level = 1; // probably not necessary
				AddPITOutput(newindex);
			}
		}
		break;
	case 4:		/* Software triggered strobe */
		spkr.pit_output_level = 1;
		AddPITOutput(newindex);
        spkr.pit_max=(1000.0f/PIT_TICK_RATE)*cntr;
		break;
	default:
#ifdef SPKR_DEBUGGING
		LOG_MSG("Unhandled speaker mode %d at %f", mode, PIC_FullIndex());
#endif
		return;
	}
	spkr.pit_mode = mode;

    /* writing the counter enables the clock again */
    PCSPEAKER_UpdateType();

    CheckPITSynchronization();
}

void PCSPEAKER_SetType(bool pit_clock_gate_enabled, bool pit_output_enabled) {
    if (spkr.chan == NULL)
        return;

    pit_raw_clock_gate_enabled = pit_clock_gate_enabled;

#ifdef SPKR_DEBUGGING
	fprintf(
			PCSpeakerLog,
			"%f output: %s, clock gate %s\n",
			PIC_FullIndex(),
			pit_output_enabled ? "pit" : "forced low",
			pit_clock_gate_enabled ? "on" : "off"
			);
	speaker_state_change_t temp;
	memset(&temp, 0, sizeof(speaker_state_change_t));
	temp.timestamp = PIC_FullIndex();
	temp.state_label = LABEL_OUTPUT;
	temp.state_specific_data.output_state.pit_clock_gate_enabled = pit_clock_gate_enabled;
	temp.state_specific_data.output_state.pit_output_enabled     = pit_output_enabled;
	if (fwrite(&temp, sizeof(temp), 1, PCSpeakerState) != 1)
		LOG_MSG(__FILE__ ": unable to write to pc speaker log");
#endif
	if (!spkr.last_ticks) {
		if(spkr.chan) spkr.chan->Enable(true);
		spkr.last_index=0;
	}
	spkr.last_ticks=PIC_Ticks;
	pic_tickindex_t newindex=PIC_TickIndex();
	ForwardPIT(newindex);
	// pit clock gate enable rising edge is a trigger

    bool p_cg_en = spkr.pit_clock_gate_enabled;

	spkr.pit_clock_gate_enabled = pit_clock_gate_enabled && TIMER2_ClockGateEnabled();
	spkr.pit_output_enabled     = pit_output_enabled;

    bool pit_trigger = !p_cg_en && spkr.pit_clock_gate_enabled;

	if (pit_trigger) {
		switch (spkr.pit_mode) {
		case 1:
			if (spkr.pit_mode1_waiting_for_counter) {
				// assert output level is high
				break;
			}
			spkr.pit_output_level = 0;
			spkr.pit_index = 0;
			spkr.pit_max = spkr.pit_mode1_pending_max;
			spkr.pit_mode1_waiting_for_trigger = 0;
			break;
		case 3:
			spkr.pit_mode3_counting = 1;
			spkr.pit_new_half=spkr.pit_new_max/2;
			spkr.pit_index = 0;
			spkr.pit_max = spkr.pit_new_max;
			spkr.pit_half = spkr.pit_new_half;
			spkr.pit_output_level = 1;
			break;
		default:
			// TODO: implement other modes
			break;
		}
	} else if (!spkr.pit_clock_gate_enabled) {
		switch (spkr.pit_mode) {
		case 1:
			// gate level does not affect mode1
			break;
		case 3:
			// low gate forces pit output high
			spkr.pit_output_level   = 1;
			spkr.pit_mode3_counting = 0;
			break;
		default:
			// TODO: implement other modes
			break;
		}
	}
	if (pit_output_enabled) {
		AddDelayEntry(newindex, spkr.pit_output_level);
	} else {
		AddDelayEntry(newindex, 0);
	}

    CheckPITSynchronization();
}

/* NTS: This code stinks. Sort of. The way it handles the delay entry queue
 *      could have been done better. The event queue idea isn't needed anymore because
 *      DOSBox-X allows any code to render audio "up to" the current time.
 *
 *      Second, looking at this code tells me why it didn't work properly with the
 *      "sample accurate" mixer mode. This code assumes that whatever length of
 *      audio there is to render, that all events within the 1ms tick interval are
 *      to be squashed and stretched to fill it. In the new DOSBox-X model mixer
 *      callback code must not assume the call is for 1ms of audio, because any
 *      code at any time can trigger a mixer render "up to" the current time with
 *      the tick. */
static void PCSPEAKER_CallBack(Bitu len) {
	bool ultrasonic = false;
	int16_t * stream=(int16_t*)MixTemp;
	ForwardPIT(1.0 + PIC_TickIndex());
    CheckPITSynchronization();
	spkr.last_index = PIC_TickIndex();
	Bitu count=len;
	Bitu pos=0;
	pic_tickindex_t sample_base=0;
	pic_tickindex_t sample_add=(pic_tickindex_t)(1.0001/len);
	while (count--) {
		pic_tickindex_t index=sample_base;
		sample_base+=sample_add;
		pic_tickindex_t end=sample_base;
		double value=0;
		while(index<end) {
			/* Check if there is an upcoming event */
			if (spkr.used && spkr.entries[pos].index<=index) {
				spkr.volwant=SPKR_VOLUME*(pic_tickindex_t)spkr.entries[pos].output_level;

				/* A change in PC speaker output means to keep rendering.
				 * Do not allow timeout to occur unless speaker is idle too long. */
				if (spkr.pit_mode == 3 && spkr.pit_max < (1000.0/spkr.rate)) {
					/* Unless the speaker is cycling at ultrasonic frequencies, meaning games
					 * that "silence" the output by setting the counter way above audible frequencies. */
					ultrasonic = true;
				}
				else {
					spkr.last_ticks=PIC_Ticks;
				}

#ifdef SPKR_DEBUGGING
				fprintf(
						PCSpeakerOutputLevelLog,
						"%f %u\n",
						PIC_Ticks + spkr.entries[pos].index,
						spkr.entries[pos].output_level);
				double tempIndex = PIC_Ticks + spkr.entries[pos].index;
				unsigned char tempOutputLevel = spkr.entries[pos].output_level;
				fwrite(&tempIndex, sizeof(double), 1, PCSpeakerOutput);
				fwrite(&tempOutputLevel, sizeof(unsigned char), 1, PCSpeakerOutput);
#endif
				pos++;spkr.used--;
				continue;
			}
			pic_tickindex_t vol_end;
			if (spkr.used && spkr.entries[pos].index<end) {
				vol_end=spkr.entries[pos].index;
			} else vol_end=end;
			pic_tickindex_t vol_len=vol_end-index;
            /* Check if we have to slide the volume */
			pic_tickindex_t vol_diff=spkr.volwant-spkr.volcur;
			if (vol_diff == 0) {
				value+=spkr.volcur*vol_len;
				index+=vol_len;
			} else {
				/* Check how long it will take to goto new level */
				pic_tickindex_t vol_time=fabs(vol_diff)/SPKR_SPEED;
				if (vol_time<=vol_len) {
					/* Volume reaches endpoint in this block, calc until that point */
					value+=vol_time*spkr.volcur;
					value+=vol_time*vol_diff/2;
					index+=vol_time;
					spkr.volcur=spkr.volwant;
				} else {
					/* Volume still not reached in this block */
					value+=spkr.volcur*vol_len;
					if (vol_diff<0) {
						value-=(SPKR_SPEED*vol_len*vol_len)/2;
						spkr.volcur-=SPKR_SPEED*vol_len;
					} else {
						value+=(SPKR_SPEED*vol_len*vol_len)/2;
						spkr.volcur+=SPKR_SPEED*vol_len;
					}
					index+=vol_len;
				}
			}
		}
		*stream++=(int16_t)(value/sample_add);
	}
	if(spkr.chan) spkr.chan->AddSamples_m16(len,(int16_t*)MixTemp);

	//Turn off speaker after 10 seconds of idle or one second idle when in off mode
	bool turnoff = false;
	Bitu test_ticks = PIC_Ticks;
	if ((spkr.last_ticks + 10000) < test_ticks) turnoff = true;
	if((!spkr.pit_output_enabled) && ((spkr.last_ticks + 1000) < test_ticks)) turnoff = true;

	if(turnoff){
		if(spkr.volwant == 0) { 
			spkr.last_ticks = 0;
			if(spkr.chan) {
				if (!spkr.pit_output_enabled)
					LOG(LOG_MISC,LOG_DEBUG)("Silencing PC speaker output (output disabled)");
				else if (ultrasonic)
					LOG(LOG_MISC,LOG_DEBUG)("Silencing PC speaker output (timeout and ultrasonic frequency)");
				else
					LOG(LOG_MISC,LOG_DEBUG)("Silencing PC speaker output (timeout and non-changing output)");

				spkr.chan->Enable(false);
			}
		} else {
			if(spkr.volwant > 0) spkr.volwant--; else spkr.volwant++;
		
		}
	}
	if (spkr.used != 0) {
        if (pos != 0) {
            /* well then roll the queue back */
            for (Bitu i=0;i < spkr.used;i++)
                spkr.entries[i] = spkr.entries[pos+i];
        }

        /* hack: some indexes come out at 1.001, fix that for the next round.
         *       this is a consequence of DOSBox-X allowing the CPU cycles
         *       count use to overrun slightly for accuracy. if we DONT fix
         *       this the delay queue will get stuck and PC speaker output
         *       will stop. */
        for (Bitu i=0;i < spkr.used;i++) {
            if (spkr.entries[i].index >= 1.000)
                spkr.entries[i].index -= 1.000;
            else
                break;
        }

        LOG(LOG_MISC,LOG_DEBUG)("PC speaker queue render, %u entries left, %u rendered",(unsigned int)spkr.used,(unsigned int)pos);
        LOG(LOG_MISC,LOG_DEBUG)("Next entry waits for index %.3f, stopped at %.3f",spkr.entries[0].index,sample_base);
	}
}
class PCSPEAKER:public Module_base {
private:
	MixerObject MixerChan;
public:
	PCSPEAKER(Section* configuration):Module_base(configuration){
		spkr.chan=0;
		Section_prop * section=static_cast<Section_prop *>(configuration);
		if(!section->Get_bool("pcspeaker")) return;
		spkr.pit_output_enabled = 0;
		spkr.pit_clock_gate_enabled = 0;
		spkr.pit_mode1_waiting_for_trigger = 1;
		spkr.last_ticks=0;
		spkr.last_index=0;
		spkr.rate=(unsigned int)section->Get_int("pcrate");

		// PIT initially in mode 3 at ~903 Hz
		spkr.pit_mode = 3;
		spkr.pit_mode3_counting = 0;
		spkr.pit_output_level = 1;
		spkr.pit_max=(1000.0f/PIT_TICK_RATE)*1320;
		spkr.pit_half=spkr.pit_max/2;
		spkr.pit_new_max=spkr.pit_max;
		spkr.pit_new_half=spkr.pit_half;
		spkr.pit_index=0;

		/* set minimum counter value with some headroom so that when games "silence" the PC speaker
		 * by setting the counter to an ultrasonic frequency, it averages out into a quiet hiss rather
		 * than noisy aliasing noise. */
		spkr.minimum_counter = PIT_TICK_RATE/(spkr.rate*10);
		SPKR_SPEED = (pic_tickindex_t)((SPKR_VOLUME*2*44100)/(0.050*spkr.rate)); /* calibrated around DOSBox-X default rate 44100h */
		spkr.used=0;
		/* Register the sound channel */
		spkr.chan=MixerChan.Install(&PCSPEAKER_CallBack,spkr.rate,"SPKR");
		if (!spkr.chan) {
			E_Exit(__FILE__ ": Unable to register channel with mixer.");
		}
		spkr.chan->SetLowpassFreq(14000);
		spkr.chan->Enable(true);
#ifdef SPKR_DEBUGGING
		PCSpeakerLog = fopen("PCSpeakerLog.txt", "w");
		if (PCSpeakerLog == NULL) {
			E_Exit(__FILE__ ": Unable to create a PC speaker log for debugging.");
		}
		PCSpeakerState = fopen("PCSpeakerState.dat", "wb");
		if (PCSpeakerState == NULL) {
			E_Exit(__FILE__ ": Unable to create a PC speaker state file for debugging.");
		}
		PCSpeakerOutput = fopen("PCSpeakerOutput.dat", "wb");
		if (PCSpeakerOutput == NULL) {
			E_Exit(__FILE__ ": Unable to create a PC speaker output file for debugging.");
		}
		PCSpeakerOutputLevelLog = fopen("PCSpeakerOutputLevelLog.txt", "w");
		if (PCSpeakerOutputLevelLog == NULL) {
			E_Exit(__FILE__ ": Unable to create a PC speaker output level log for debugging.");
		}
#endif
	}
};

static PCSPEAKER* test;

void PCSPEAKER_ShutDown(Section* sec){
    (void)sec;//UNUSED
	delete test;
}

void PCSPEAKER_OnReset(Section* sec) {
    (void)sec;//UNUSED
	if (test == NULL) {
		LOG(LOG_MISC,LOG_DEBUG)("Allocating PC speaker emulation");
		test = new PCSPEAKER(control->GetSection("speaker"));
	}
}

void PCSPEAKER_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing PC speaker");

	AddExitFunction(AddExitFunctionFuncPair(PCSPEAKER_ShutDown),true);
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(PCSPEAKER_OnReset));
}

// save state support
void POD_Save_PCSpeaker( std::ostream& stream )
{
	const char pod_name[32] = "PCSpeaker";

	if( stream.fail() ) return;
	if( !test ) return;
	if( !spkr.chan ) return;


	WRITE_POD( &pod_name, pod_name );

	//*******************************************
	//*******************************************
	//*******************************************

	// - near-pure data
	WRITE_POD( &spkr, spkr );

	//*******************************************
	//*******************************************
	//*******************************************

	spkr.chan->SaveState(stream);
}


void POD_Load_PCSpeaker( std::istream& stream )
{
	char pod_name[32] = {0};

	if( stream.fail() ) return;
	if( !test ) return;
	if( !spkr.chan ) return;


	// error checking
	READ_POD( &pod_name, pod_name );
	if( strcmp( pod_name, "PCSpeaker" ) ) {
		stream.clear( std::istream::failbit | std::istream::badbit );
		return;
	}

	//************************************************
	//************************************************
	//************************************************

	MixerChannel *chan_old;


	// - save static ptrs
	chan_old = spkr.chan;

	//*******************************************
	//*******************************************
	//*******************************************

	// - near-pure data
	READ_POD( &spkr, spkr );

	//*******************************************
	//*******************************************
	//*******************************************

	// - restore static ptrs
	spkr.chan = chan_old;


	spkr.chan->LoadState(stream);
}
