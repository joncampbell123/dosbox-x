/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */


/* 
    Remove the sdl code from here and have it handeld in the sdlmain.
    That should call the mixer start from there or something.
*/

#include <string.h>
#include <sys/types.h>
#define _USE_MATH_DEFINES // needed for M_PI in Visual Studio as documented [https://msdn.microsoft.com/en-us/library/4hwaceh6.aspx]
#include <math.h>

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

#if defined (WIN32)
//Midi listing
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

#if !defined(M_PI)
# define M_PI (3.141592654)
#endif

#include "SDL.h"
#include "mem.h"
#include "pic.h"
#include "dosbox.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "cross.h"
#include "support.h"
#include "control.h"
#include "mapper.h"
#include "hardware.h"
#include "programs.h"
#include "midi.h"

#define MIXER_SSIZE 4
#define MIXER_VOLSHIFT 13

static INLINE Bit16s MIXER_CLIP(Bits SAMP) {
    if (SAMP < MAX_AUDIO) {
        if (SAMP > MIN_AUDIO)
            return SAMP;
        else
            return MIN_AUDIO;
    } else {
        return MAX_AUDIO;
    }
}

struct mixedFraction {
    unsigned int        w;
    unsigned int        fn,fd;
};

static struct {
    Bit32s          work[MIXER_BUFSIZE][2];
    Bitu            work_in,work_out,work_wrap;
    Bitu            pos,done;
    float           mastervol[2];
    float           recordvol[2];
    MixerChannel*       channels;
    Bit32u          freq;
    Bit32u          blocksize;
    struct mixedFraction samples_per_ms;
    struct mixedFraction samples_this_ms;
    struct mixedFraction samples_rendered_ms;
    bool            nosound;
    bool            swapstereo;
    bool            sampleaccurate;
    bool            prebuffer_wait;
    Bitu            prebuffer_samples;
    bool            mute;
} mixer;

uint32_t Mixer_MIXQ(void) {
    return  ((uint32_t)mixer.freq) |
            ((uint32_t)2u/*channels*/ << (uint32_t)20u) |
            (mixer.swapstereo ?      ((uint32_t)1u << (uint32_t)29u) : 0u) |
            (mixer.mute       ?      ((uint32_t)1u << (uint32_t)30u) : 0u) |
            (mixer.nosound    ? 0u : ((uint32_t)1u << (uint32_t)31u));
}

PhysPt mixer_capture_write = 0;
PhysPt mixer_capture_write_begin = 0;
PhysPt mixer_capture_write_end = 0;
uint32_t mixer_control = 0;

// mixer capture source bits [23:16]
enum {
    MIXER_SRC_MIXDOWN=0
};

unsigned int Mixer_MIXC_Source(void) {
    return (unsigned int)((mixer_control >> 16ul) & 0xFFul);
}

bool Mixer_MIXC_Active(void) {
    return ((mixer_control & 3u) == 3u)/*capture interface enable|write to memory*/;
}

bool Mixer_MIXC_Error(void) {
    return ((mixer_control & 8u) == 8u);
}

bool Mixer_MIXC_ShouldLoop(void) {
    return ((mixer_control & 4u) == 4u);
}

void Mixer_MIXC_Stop(void) {
    mixer_control &= ~1u; // clear enable
}

void Mixer_MIXC_LoopAround(void) {
    mixer_capture_write = mixer_capture_write_begin;
}

// NTS: Check AFTER writing sample
bool Mixer_MIXC_AtEnd(void) {
    return (mixer_capture_write >= mixer_capture_write_end);
}

void Mixer_MIXC_MarkError(void) {
    mixer_control &= ~1u; // clear enable
    mixer_control |=  8u; // set error
}

PhysPt Mixer_MIXWritePos(void) {
    return mixer_capture_write;
}

void Mixer_MIXWritePos_Write(PhysPt np) {
    if (!Mixer_MIXC_Active())
        mixer_capture_write = np;
}

void Mixer_MIXWriteBegin_Write(PhysPt np) {
    if (!Mixer_MIXC_Active())
        mixer_capture_write_begin = np;
}

void Mixer_MIXWriteEnd_Write(PhysPt np) {
    if (!Mixer_MIXC_Active())
        mixer_capture_write_end = np;
}

void Mixer_MIXC_Validate(void) {
    if (Mixer_MIXC_Active()) {
        // NTS: phys_writew() will cause a segfault if the address is beyond the end of memory,
        //      because it computes MemBase+addr
        PhysPt MemMax = (PhysPt)MEM_TotalPages() * (PhysPt)4096ul;

        if (Mixer_MIXC_Error() ||
            Mixer_MIXC_Source() != 0x00 ||
            mixer_capture_write == 0 || mixer_capture_write_begin == 0 || mixer_capture_write_end == 0 ||
            mixer_capture_write < mixer_capture_write_begin ||
            mixer_capture_write > mixer_capture_write_end ||
            mixer_capture_write_begin > mixer_capture_write_end ||
            mixer_capture_write >= MemMax ||
            mixer_capture_write_end >= MemMax ||
            mixer_capture_write_begin >= MemMax)
            Mixer_MIXC_MarkError();
    }
}

uint32_t Mixer_MIXC(void) {
    return mixer_control;
}

void Mixer_MIXC_Write(uint32_t v) {
    /* bit [0:0] = enable capture interface
     * bit [1:1] = enable writing to memory
     * bit [2:2] = enable loop around, when write == write_end, set write == write_begin
     * bit [3:3] = 1=error condition  0=no error
     * bit [23:16] = source selection (see list) */
    if (mixer_control != v) {
        mixer_control = (v & 0x00FF00FFUL);
        Mixer_MIXC_Validate();
    }
}

bool Mixer_SampleAccurate() {
    return mixer.sampleaccurate;
}

Bit8u MixTemp[MIXER_BUFSIZE];

inline void MixerChannel::updateSlew(void) {
    /* "slew" affects the linear interpolation ramp.
     * but, our implementation can only shorten the linear interpolation
     * period, it cannot extend it beyond one sample period */
    freq_nslew = freq_nslew_want;
    if (freq_nslew < freq_n) freq_nslew = freq_n;

    if (freq_nslew_want > 0 && freq_nslew_want < freq_n)
        max_change = ((Bit64u)freq_nslew_want * (Bit64u)0x8000) / (Bit64u)freq_n;
    else
        max_change = 0x7FFFFFFFUL;
}

MixerChannel * MIXER_AddChannel(MIXER_Handler handler,Bitu freq,const char * name) {
    MixerChannel * chan=new MixerChannel();
    chan->scale = 1.0;
    chan->freq_fslew = 0;
    chan->freq_nslew_want = 0;
    chan->freq_nslew = 0;
    chan->last_sample_write = 0;
    chan->current_loaded = false;
    chan->handler=handler;
    chan->name=name;
    chan->msbuffer_i = 0;
    chan->msbuffer_o = 0;
    chan->freq_n = chan->freq_d = 1;
    chan->lowpass_freq = 0;
    chan->lowpass_alpha = 0;

    for (unsigned int i=0;i < LOWPASS_ORDER;i++) {
        for (unsigned int j=0;j < 2;j++)
            chan->lowpass[i][j] = 0;
    }

    chan->lowpass_on_load = false;
    chan->lowpass_on_out = false;
    chan->freq_d_orig = 1;
    chan->freq_f = 0;
    chan->SetFreq(freq);
    chan->next=mixer.channels;
    chan->SetVolume(1,1);
    chan->enabled=false;
    chan->last[0] = chan->last[1] = 0;
    chan->delta[0] = chan->delta[1] = 0;
    chan->current[0] = chan->current[1] = 0;

    mixer.channels=chan;
    return chan;
}

MixerChannel * MIXER_FirstChannel(void) {
    return mixer.channels;
}

MixerChannel * MIXER_FindChannel(const char * name) {
    MixerChannel * chan=mixer.channels;
    while (chan) {
        if (!strcasecmp(chan->name,name)) break;
        chan=chan->next;
    }
    return chan;
}

void MIXER_DelChannel(MixerChannel* delchan) {
    MixerChannel * chan=mixer.channels;
    MixerChannel * * where=&mixer.channels;
    while (chan) {
        if (chan==delchan) {
            *where=chan->next;
            delete delchan;
            return;
        }
        where=&chan->next;
        chan=chan->next;
    }
}

void MixerChannel::UpdateVolume(void) {
    volmul[0]=(Bits)((1 << MIXER_VOLSHIFT)*scale*volmain[0]);
    volmul[1]=(Bits)((1 << MIXER_VOLSHIFT)*scale*volmain[1]);
}

void MixerChannel::SetVolume(float _left,float _right) {
    volmain[0]=_left;
    volmain[1]=_right;
    UpdateVolume();
}

void MixerChannel::SetScale( float f ) {
    scale = f;
    UpdateVolume();
}

static void MIXER_FillUp(void);

void MixerChannel::Enable(bool _yesno) {
    if (_yesno==enabled) return;
    enabled=_yesno;
    if (!enabled) freq_f=0;
}

void MixerChannel::lowpassUpdate() {
    if (lowpass_freq != 0) {
        double timeInterval;
        double tau,talpha;

        if (freq_n > freq_d) { // source -> dest mixer rate ratio is > 100%
            timeInterval = (double)freq_d_orig / freq_n; // will filter on sample load at source rate
            lowpass_on_load = true;
            lowpass_on_out = false;
        }
        else {
            timeInterval = (double)1.0 / mixer.freq; // will filter mixer output at mixer rate
            lowpass_on_load = false;
            lowpass_on_out = true;
        }

        tau = 1.0 / (lowpass_freq * 2 * M_PI);
        talpha = timeInterval / (tau + timeInterval);
        lowpass_alpha = (Bit32s)(talpha * 0x10000); // double -> 16.16 fixed point

//      LOG_MSG("Lowpass freq_n=%u freq_d=%u timeInterval=%.12f tau=%.12f alpha=%.6f onload=%u onout=%u",
//          freq_n,freq_d_orig,timeInterval,tau,talpha,lowpass_on_load,lowpass_on_out);
    }
    else {
        lowpass_on_load = false;
        lowpass_on_out = false;
    }
}

inline Bit32s MixerChannel::lowpassStep(Bit32s in,const unsigned int iteration,const unsigned int channel) {
    const Bit64s m1 = (Bit64s)in * (Bit64s)lowpass_alpha;
    const Bit64s m2 = ((Bit64s)lowpass[iteration][channel] << ((Bit64s)16)) - ((Bit64s)lowpass[iteration][channel] * (Bit64s)lowpass_alpha);
    const Bit32s ns = (Bit32s)((m1 + m2) >> (Bit64s)16);
    lowpass[iteration][channel] = ns;
    return ns;
}

inline void MixerChannel::lowpassProc(Bit32s ch[2]) {
    for (unsigned int i=0;i < lowpass_order;i++) {
        for (unsigned int c=0;c < 2;c++)
            ch[c] = lowpassStep(ch[c],i,c);
    }
}

void MixerChannel::SetLowpassFreq(Bitu _freq,unsigned int order) {
    if (order > LOWPASS_ORDER) order = LOWPASS_ORDER;
    if (_freq == lowpass_freq && lowpass_order == order) return;
    lowpass_order = order;
    lowpass_freq = _freq;
    lowpassUpdate();
}

void MixerChannel::SetSlewFreq(Bitu _freq) {
    freq_nslew_want = _freq;
    updateSlew();
}

void MixerChannel::SetFreq(Bitu _freq,Bitu _den) {
    if (freq_n == _freq && freq_d == freq_d_orig)
        return;

    if (freq_d_orig != _den) {
        Bit64u tmp = (Bit64u)freq_f * (Bit64u)_den * (Bit64u)mixer.freq;
        freq_f = freq_fslew = (unsigned int)(tmp / (Bit64u)freq_d_orig);
    }

    freq_n = _freq;
    freq_d = _den * mixer.freq;
    freq_d_orig = _den;
    updateSlew();
    lowpassUpdate();
}

void CAPTURE_MultiTrackAddWave(Bit32u freq, Bit32u len, Bit16s * data,const char *name);

void MixerChannel::EndFrame(Bitu samples) {
    if (CaptureState & CAPTURE_MULTITRACK_WAVE) {// TODO: should be a separate call!
        Bit16s convert[1024][2];
        Bitu cnv = msbuffer_o;
        Bitu padding = 0;

        if (cnv > samples)
            cnv = samples;
        else
            padding = samples - cnv;

        if (cnv > 0) {
            Bit32s volscale1 = (Bit32s)(mixer.recordvol[0] * (1 << MIXER_VOLSHIFT));
            Bit32s volscale2 = (Bit32s)(mixer.recordvol[1] * (1 << MIXER_VOLSHIFT));

            if (cnv > 1024) cnv = 1024;
            for (Bitu i=0;i<cnv;i++) {
                convert[i][0]=MIXER_CLIP(((Bit64s)msbuffer[i][0] * (Bit64s)volscale1) >> (MIXER_VOLSHIFT + MIXER_VOLSHIFT));
                convert[i][1]=MIXER_CLIP(((Bit64s)msbuffer[i][1] * (Bit64s)volscale2) >> (MIXER_VOLSHIFT + MIXER_VOLSHIFT));
            }
            CAPTURE_MultiTrackAddWave(mixer.freq,cnv,(Bit16s*)convert,name);
        }

        if (padding > 0) {
            if (padding > 1024) padding = 1024;
            memset(&convert[0][0],0,padding*sizeof(Bit16s)*2);
            CAPTURE_MultiTrackAddWave(mixer.freq,padding,(Bit16s*)convert,name);
        }
    }

    rend_n = rend_d = 0;
    if (msbuffer_o <= samples) {
        msbuffer_o = 0;
        msbuffer_i = 0;
    }
    else {
        msbuffer_o -= samples;
        if (msbuffer_i >= samples) msbuffer_i -= samples;
        else msbuffer_i = 0;
        memmove(&msbuffer[0][0],&msbuffer[samples][0],msbuffer_o*sizeof(Bit32s)*2/*stereo*/);
    }

    last_sample_write -= (int)samples;
}

void MixerChannel::Mix(Bitu whole,Bitu frac) {
    unsigned int patience = 2;
    Bitu upto;

    if (whole <= rend_n) return;
    assert(whole <= mixer.samples_this_ms.w);
    assert(rend_n < mixer.samples_this_ms.w);
    Bit32s *outptr = &mixer.work[mixer.work_in+rend_n][0];

    if (!enabled) {
        rend_n = whole;
        rend_d = frac;
        return;
    }

    // HACK: We iterate twice only because of the Sound Blaster emulation. No other emulation seems to need this.
    rendering_to_n = whole;
    rendering_to_d = frac;
    while (msbuffer_o < whole) {
        Bit64u todo = (Bit64u)(whole - msbuffer_o) * (Bit64u)freq_n;
        todo += (Bit64u)freq_f;
        todo += (Bit64u)freq_d - (Bit64u)1;
        todo /= (Bit64u)freq_d;
        if (!current_loaded) todo++;
        handler(todo);

        if (--patience == 0) break;
    }

    if (msbuffer_o < whole)
        padFillSampleInterpolation(whole);

    upto = whole;
    if (upto > msbuffer_o) upto = msbuffer_o;

    if (lowpass_on_out) { /* before rendering out to mixer, process samples with lowpass filter */
        Bitu t_rend_n = rend_n;
        Bitu t_msbuffer_i = msbuffer_i;

        while (t_rend_n < whole && t_msbuffer_i < upto) {
            lowpassProc(msbuffer[t_msbuffer_i]);
            t_msbuffer_i++;
            t_rend_n++;
        }
    }

    if (mixer.swapstereo) {
        while (rend_n < whole && msbuffer_i < upto) {
            *outptr++ += msbuffer[msbuffer_i][1];
            *outptr++ += msbuffer[msbuffer_i][0];
            msbuffer_i++;
            rend_n++;
        }
    }
    else {
        while (rend_n < whole && msbuffer_i < upto) {
            *outptr++ += msbuffer[msbuffer_i][0];
            *outptr++ += msbuffer[msbuffer_i][1];
            msbuffer_i++;
            rend_n++;
        }
    }

    rend_n = whole;
    rend_d = frac;
}

void MixerChannel::AddSilence(void) {
}

template<class Type,bool stereo,bool signeddata,bool nativeorder,bool T_lowpass>
inline void MixerChannel::loadCurrentSample(Bitu &len, const Type* &data) {
    last[0] = current[0];
    last[1] = current[1];

    if (sizeof(Type) == 1) {
        const uint8_t xr = signeddata ? 0x00 : 0x80;

        len--;
        current[0] = ((Bit8s)((*data++) ^ xr)) << 8;
        if (stereo)
            current[1] = ((Bit8s)((*data++) ^ xr)) << 8;
        else
            current[1] = current[0];
    }
    else if (sizeof(Type) == 2) {
        const uint16_t xr = signeddata ? 0x0000 : 0x8000;
        uint16_t d;

        len--;
        if (nativeorder) d = ((Bit16u)((*data++) ^ xr));
        else d = host_readw((HostPt)(data++)) ^ xr;
        current[0] = (Bit16s)d;
        if (stereo) {
            if (nativeorder) d = ((Bit16u)((*data++) ^ xr));
            else d = host_readw((HostPt)(data++)) ^ xr;
            current[1] = (Bit16s)d;
        }
        else {
            current[1] = current[0];
        }
    }
    else if (sizeof(Type) == 4) {
        const uint32_t xr = signeddata ? 0x00000000UL : 0x80000000UL;
        uint32_t d;

        len--;
        if (nativeorder) d = ((Bit32u)((*data++) ^ xr));
        else d = host_readd((HostPt)(data++)) ^ xr;
        current[0] = (Bit32s)d;
        if (stereo) {
            if (nativeorder) d = ((Bit32u)((*data++) ^ xr));
            else d = host_readd((HostPt)(data++)) ^ xr;
            current[1] = (Bit32s)d;
        }
        else {
            current[1] = current[0];
        }
    }
    else {
        current[0] = current[1] = 0;
        len = 0;
    }

    if (T_lowpass && lowpass_on_load)
        lowpassProc(current);

    if (stereo) {
        delta[0] = current[0] - last[0];
        delta[1] = current[1] - last[1];
    }
    else {
        delta[1] = delta[0] = current[0] - last[0];
    }

    if (freq_nslew_want != 0) {
        if (delta[0] < -max_change) delta[0] = -max_change;
        else if (delta[0] > max_change) delta[0] = max_change;

        if (stereo) {
            if (delta[1] < -max_change) delta[1] = -max_change;
            else if (delta[1] > max_change) delta[1] = max_change;
        }
        else {
            delta[1] = delta[0];
        }
    }

    current_loaded = true;
}

inline void MixerChannel::padFillSampleInterpolation(const Bitu upto) {
    finishSampleInterpolation(upto);
    if (msbuffer_o < upto) {
        if (freq_f > freq_d) freq_f = freq_d; // this is an abrupt stop, so interpolation must not carry over, to help avoid popping artifacts

        while (msbuffer_o < upto) {
            msbuffer[msbuffer_o][0] = current[0];
            msbuffer[msbuffer_o][1] = current[1];
            msbuffer_o++;
        }
    }
}

void MixerChannel::finishSampleInterpolation(const Bitu upto) {
    if (!current_loaded) return;
    runSampleInterpolation(upto);
}

double MixerChannel::timeSinceLastSample(void) {
    Bits delta = (Bits)mixer.samples_rendered_ms.w - (Bits)last_sample_write;
    return ((double)delta) / mixer.freq;
}

inline bool MixerChannel::runSampleInterpolation(const Bitu upto) {
    if (msbuffer_o >= upto)
        return false;

    while (freq_fslew < freq_d) {
        int sample = last[0] + (int)(((int64_t)delta[0] * (int64_t)freq_fslew) / (int64_t)freq_d);
        msbuffer[msbuffer_o][0] = sample * volmul[0];
        sample = last[1] + (int)(((int64_t)delta[1] * (int64_t)freq_fslew) / (int64_t)freq_d);
        msbuffer[msbuffer_o][1] = sample * volmul[1];

        freq_f += freq_n;
        freq_fslew += freq_nslew;
        if ((++msbuffer_o) >= upto)
            return false;
    }

    current[0] = last[0] + delta[0];
    current[1] = last[1] + delta[1];
    while (freq_f < freq_d) {
        msbuffer[msbuffer_o][0] = current[0] * volmul[0];
        msbuffer[msbuffer_o][1] = current[1] * volmul[1];

        freq_f += freq_n;
        if ((++msbuffer_o) >= upto)
            return false;
    }

    return true;
}

template<class Type,bool stereo,bool signeddata,bool nativeorder>
inline void MixerChannel::AddSamples(Bitu len, const Type* data) {
    last_sample_write = (Bits)mixer.samples_rendered_ms.w;

    if (msbuffer_o >= 2048) {
        fprintf(stderr,"WARNING: addSample overrun (immediate)\n");
        return;
    }

    if (!current_loaded) {
        if (len == 0) return;

        loadCurrentSample<Type,stereo,signeddata,nativeorder,false>(len,data);
        if (len == 0) {
            freq_f = freq_fslew = freq_d; /* encourage loading next round */
            return;
        }

        loadCurrentSample<Type,stereo,signeddata,nativeorder,false>(len,data);
        freq_f = freq_fslew = 0; /* interpolate now from what we just loaded */
    }

    if (lowpass_on_load) {
        for (;;) {
            if (freq_f >= freq_d) {
                if (len == 0) break;
                loadCurrentSample<Type,stereo,signeddata,nativeorder,true>(len,data);
                freq_f -= freq_d;
                freq_fslew = freq_f;
            }
            if (!runSampleInterpolation(2048))
                break;
        }
    }
    else {
        for (;;) {
            if (freq_f >= freq_d) {
                if (len == 0) break;
                loadCurrentSample<Type,stereo,signeddata,nativeorder,false>(len,data);
                freq_f -= freq_d;
                freq_fslew = freq_f;
            }
            if (!runSampleInterpolation(2048))
                break;
        }
    }
}

void MixerChannel::AddSamples_m8(Bitu len, const Bit8u * data) {
    AddSamples<Bit8u,false,false,true>(len,data);
}
void MixerChannel::AddSamples_s8(Bitu len,const Bit8u * data) {
    AddSamples<Bit8u,true,false,true>(len,data);
}
void MixerChannel::AddSamples_m8s(Bitu len,const Bit8s * data) {
    AddSamples<Bit8s,false,true,true>(len,data);
}
void MixerChannel::AddSamples_s8s(Bitu len,const Bit8s * data) {
    AddSamples<Bit8s,true,true,true>(len,data);
}
void MixerChannel::AddSamples_m16(Bitu len,const Bit16s * data) {
    AddSamples<Bit16s,false,true,true>(len,data);
}
void MixerChannel::AddSamples_s16(Bitu len,const Bit16s * data) {
    AddSamples<Bit16s,true,true,true>(len,data);
}
void MixerChannel::AddSamples_m16u(Bitu len,const Bit16u * data) {
    AddSamples<Bit16u,false,false,true>(len,data);
}
void MixerChannel::AddSamples_s16u(Bitu len,const Bit16u * data) {
    AddSamples<Bit16u,true,false,true>(len,data);
}
void MixerChannel::AddSamples_m32(Bitu len,const Bit32s * data) {
    AddSamples<Bit32s,false,true,true>(len,data);
}
void MixerChannel::AddSamples_s32(Bitu len,const Bit32s * data) {
    AddSamples<Bit32s,true,true,true>(len,data);
}
void MixerChannel::AddSamples_m16_nonnative(Bitu len,const Bit16s * data) {
    AddSamples<Bit16s,false,true,false>(len,data);
}
void MixerChannel::AddSamples_s16_nonnative(Bitu len,const Bit16s * data) {
    AddSamples<Bit16s,true,true,false>(len,data);
}
void MixerChannel::AddSamples_m16u_nonnative(Bitu len,const Bit16u * data) {
    AddSamples<Bit16u,false,false,false>(len,data);
}
void MixerChannel::AddSamples_s16u_nonnative(Bitu len,const Bit16u * data) {
    AddSamples<Bit16u,true,false,false>(len,data);
}
void MixerChannel::AddSamples_m32_nonnative(Bitu len,const Bit32s * data) {
    AddSamples<Bit32s,false,true,false>(len,data);
}
void MixerChannel::AddSamples_s32_nonnative(Bitu len,const Bit32s * data) {
    AddSamples<Bit32s,true,true,false>(len,data);
}

extern bool ticksLocked;

#if 0//unused
static inline bool Mixer_irq_important(void) {
    /* In some states correct timing of the irqs is more important then 
     * non stuttering audo */
    return (ticksLocked || (CaptureState & (CAPTURE_WAVE|CAPTURE_VIDEO|CAPTURE_MULTITRACK_WAVE)));
}
#endif

unsigned long long mixer_sample_counter = 0;
double mixer_start_pic_time = 0;

/* once a millisecond, render 1ms of audio, up to whole samples */
static void MIXER_MixData(Bitu fracs/*render up to*/) {
    unsigned int prev_rendered = mixer.samples_rendered_ms.w;
    MixerChannel *chan = mixer.channels;
    unsigned int whole,frac;
    bool endframe = false;

    if (fracs >= ((Bitu)mixer.samples_this_ms.w * mixer.samples_this_ms.fd)) {
        fracs = ((Bitu)mixer.samples_this_ms.w * mixer.samples_this_ms.fd);
        endframe = true;
    }

    whole = (unsigned int)(fracs / mixer.samples_this_ms.fd);
    frac = (unsigned int)(fracs % mixer.samples_this_ms.fd);
    if (whole <= mixer.samples_rendered_ms.w) return;

    while (chan) {
        chan->Mix(whole,fracs);
        if (endframe) chan->EndFrame(mixer.samples_this_ms.w);
        chan=chan->next;
    }

    if (CaptureState & (CAPTURE_WAVE|CAPTURE_VIDEO)) {
        Bit32s volscale1 = (Bit32s)(mixer.recordvol[0] * (1 << MIXER_VOLSHIFT));
        Bit32s volscale2 = (Bit32s)(mixer.recordvol[1] * (1 << MIXER_VOLSHIFT));
        Bit16s convert[1024][2];
        Bitu added = whole - prev_rendered;
        if (added>1024) added=1024;
        Bitu readpos = mixer.work_in + prev_rendered;
        for (Bitu i=0;i<added;i++) {
            convert[i][0]=MIXER_CLIP(((Bit64s)mixer.work[readpos][0] * (Bit64s)volscale1) >> (MIXER_VOLSHIFT + MIXER_VOLSHIFT));
            convert[i][1]=MIXER_CLIP(((Bit64s)mixer.work[readpos][1] * (Bit64s)volscale2) >> (MIXER_VOLSHIFT + MIXER_VOLSHIFT));
            readpos++;
        }
        assert(readpos <= MIXER_BUFSIZE);
        CAPTURE_AddWave( mixer.freq, added, (Bit16s*)convert );
    }

    if (Mixer_MIXC_Active() && prev_rendered < whole) {
        Bitu readpos = mixer.work_in + prev_rendered;
        Bitu added = whole - prev_rendered;
        Bitu cando = (mixer_capture_write_end - mixer_capture_write) / 2/*bytes/sample*/ / 2/*channels*/;
        if (cando > added) cando = added;

        if (cando == 0 && !Mixer_MIXC_AtEnd()) {
            Mixer_MIXC_MarkError();
        }
        else if (cando != 0) {
            for (Bitu i=0;i < cando;i++) {
                phys_writew(mixer_capture_write,(Bit16u)MIXER_CLIP(((Bit64s)mixer.work[readpos][0]) >> (MIXER_VOLSHIFT)));
                mixer_capture_write += 2;

                phys_writew(mixer_capture_write,(Bit16u)MIXER_CLIP(((Bit64s)mixer.work[readpos][1]) >> (MIXER_VOLSHIFT)));
                mixer_capture_write += 2;

                readpos++;
            }

            if (Mixer_MIXC_AtEnd()) {
                if (Mixer_MIXC_ShouldLoop())
                    Mixer_MIXC_LoopAround();
                else
                    Mixer_MIXC_Stop();
            }
        }
    }

    mixer.samples_rendered_ms.w = whole;
    mixer.samples_rendered_ms.fd = frac;
    mixer_sample_counter += mixer.samples_rendered_ms.w - prev_rendered;
}

static void MIXER_FillUp(void) {
    SDL_LockAudio();
    float index = PIC_TickIndex();
    if (index < 0) index = 0;
    MIXER_MixData((Bitu)((double)index * ((Bitu)mixer.samples_this_ms.w * mixer.samples_this_ms.fd)));
    SDL_UnlockAudio();
}

void MixerChannel::FillUp(void) {
    MIXER_FillUp();
}

void MIXER_MixSingle(Bitu /*val*/) {
    MIXER_FillUp();
    PIC_AddEvent(MIXER_MixSingle,1000.0 / mixer.freq);
}

static void MIXER_Mix(void) {
    Bitu thr;

    SDL_LockAudio();

    /* render */
    assert((mixer.work_in+mixer.samples_per_ms.w) <= MIXER_BUFSIZE);
    MIXER_MixData((Bitu)mixer.samples_this_ms.w * (Bitu)mixer.samples_this_ms.fd);
    mixer.work_in += mixer.samples_this_ms.w;

    /* how many samples for the next ms? */
    mixer.samples_this_ms.w = mixer.samples_per_ms.w;
    mixer.samples_this_ms.fn += mixer.samples_per_ms.fn;
    if (mixer.samples_this_ms.fn >= mixer.samples_this_ms.fd) {
        mixer.samples_this_ms.fn -= mixer.samples_this_ms.fd;
        mixer.samples_this_ms.w++;
    }

    /* advance. we use in/out & wrap pointers to make sure the rendering code
     * doesn't have to worry about circular buffer wraparound. */
    thr = mixer.blocksize;
    if (thr < mixer.samples_this_ms.w) thr = mixer.samples_this_ms.w;
    if ((mixer.work_in+thr) > MIXER_BUFSIZE) {
        mixer.work_wrap = mixer.work_in;
        mixer.work_in = 0;
    }
    assert((mixer.work_in+thr) <= MIXER_BUFSIZE);
    assert((mixer.work_in+mixer.samples_this_ms.w) <= MIXER_BUFSIZE);
    memset(&mixer.work[mixer.work_in][0],0,sizeof(Bit32s)*2*mixer.samples_this_ms.w);
    mixer.samples_rendered_ms.fn = 0;
    mixer.samples_rendered_ms.w = 0;
    SDL_UnlockAudio();
    MIXER_FillUp();
}

static void SDLCALL MIXER_CallBack(void * userdata, Uint8 *stream, int len) {
    (void)userdata;//UNUSED
    Bit32s volscale1 = (Bit32s)(mixer.mastervol[0] * (1 << MIXER_VOLSHIFT));
    Bit32s volscale2 = (Bit32s)(mixer.mastervol[1] * (1 << MIXER_VOLSHIFT));
    Bitu need = (Bitu)len/MIXER_SSIZE;
    Bit16s *output = (Bit16s*)stream;
    int remains;

    if (mixer.mute) {
        if ((CaptureState & (CAPTURE_WAVE|CAPTURE_VIDEO|CAPTURE_MULTITRACK_WAVE)) != 0)
            mixer.work_out = mixer.work_in;
        else
            mixer.work_out = mixer.work_in = 0;
    }

    if (mixer.prebuffer_wait) {
        remains = (int)mixer.work_in - (int)mixer.work_out;
        if (remains < 0) remains += (int)mixer.work_wrap;
        if (remains < 0) remains = 0;

        if ((unsigned int)remains >= mixer.prebuffer_samples)
            mixer.prebuffer_wait = false;
    }

    if (!mixer.prebuffer_wait && !mixer.mute) {
        Bit32s *in = &mixer.work[mixer.work_out][0];
        while (need > 0) {
            if (mixer.work_out == mixer.work_in) break;
            *output++ = MIXER_CLIP((((Bit64s)(*in++)) * (Bit64s)volscale1) >> (MIXER_VOLSHIFT + MIXER_VOLSHIFT));
            *output++ = MIXER_CLIP((((Bit64s)(*in++)) * (Bit64s)volscale2) >> (MIXER_VOLSHIFT + MIXER_VOLSHIFT));
            mixer.work_out++;
            if (mixer.work_out >= mixer.work_wrap) {
                mixer.work_out = 0;
                in = &mixer.work[mixer.work_out][0];
            }
            need--;
        }
    }

    if (need > 0)
        mixer.prebuffer_wait = true;

    while (need > 0) {
        *output++ = 0;
        *output++ = 0;
        need--;
    }

    remains = (int)mixer.work_in - (int)mixer.work_out;
    if (remains < 0) remains += (int)mixer.work_wrap;

    if ((unsigned long)remains >= (mixer.blocksize*2UL)) {
        /* drop some samples to keep time */
        unsigned int drop;

        if ((unsigned long)remains >= (mixer.blocksize*3UL)) // hard drop
            drop = ((unsigned int)remains - (unsigned int)(mixer.blocksize));
        else // subtle drop
            drop = (((unsigned int)remains - (unsigned int)(mixer.blocksize*2)) / 50U) + 1;

        while (drop > 0) {
            mixer.work_out++;
            if (mixer.work_out >= mixer.work_wrap) mixer.work_out = 0;
            drop--;
        }
    }
}

static void MIXER_Stop(Section* sec) {
    (void)sec;//UNUSED
}

class MIXER : public Program {
public:
    void MakeVolume(char * scan,float & vol0,float & vol1) {
        Bitu w=0;
        bool db=(toupper(*scan)=='D');
        if (db) scan++;
        while (*scan) {
            if (*scan==':') {
                ++scan;w=1;
            }
            char * before=scan;
            float val=(float)strtod(scan,&scan);
            if (before==scan) {
                ++scan;continue;
            }
            if (!db) val/=100;
            else val=powf(10.0f,(float)val/20.0f);
            if (val<0) val=1.0f;
            if (!w) {
                vol0=val;
            } else {
                vol1=val;
            }
        }
        if (!w) vol1=vol0;
    }

    void Run(void) {
        if (cmd->FindExist("-?", false) || cmd->FindExist("/?", false)) {
			WriteOut("Displays or changes the current sound levels.\n\nMIXER [option]\n");
            return;
		}
        if(cmd->FindExist("/LISTMIDI")) {
            ListMidi();
            return;
        }
        if (cmd->FindString("MASTER",temp_line,false)) {
            MakeVolume((char *)temp_line.c_str(),mixer.mastervol[0],mixer.mastervol[1]);
        }
        if (cmd->FindString("RECORD",temp_line,false)) {
            MakeVolume((char *)temp_line.c_str(),mixer.recordvol[0],mixer.recordvol[1]);
        }
        MixerChannel * chan=mixer.channels;
        while (chan) {
            if (cmd->FindString(chan->name,temp_line,false)) {
                MakeVolume((char *)temp_line.c_str(),chan->volmain[0],chan->volmain[1]);
            }
            chan->UpdateVolume();
            chan=chan->next;
        }
        if (cmd->FindExist("/NOSHOW")) return;
        WriteOut("Channel  Main    Main(dB)\n");
        ShowVolume("MASTER",mixer.mastervol[0],mixer.mastervol[1]);
        ShowVolume("RECORD",mixer.recordvol[0],mixer.recordvol[1]);
        for (chan=mixer.channels;chan;chan=chan->next) 
            ShowVolume(chan->name,chan->volmain[0],chan->volmain[1]);
    }
private:
    void ShowVolume(const char * name,float vol0,float vol1) {
        WriteOut("%-8s %3.0f:%-3.0f  %+3.2f:%-+3.2f \n",name,
            (double)vol0*100,(double)vol1*100,
            20*log(vol0)/log(10.0f),20*log(vol1)/log(10.0f)
        );
    }

    void ListMidi(){
        if(midi.handler) midi.handler->ListAll(this);
    };
};

static void MIXER_ProgramStart(Program * * make) {
    *make=new MIXER;
}

MixerChannel* MixerObject::Install(MIXER_Handler handler,Bitu freq,const char * name){
    if(!installed) {
        if(strlen(name) > 31) E_Exit("Too long mixer channel name");
        safe_strncpy(m_name,name,32);
        installed = true;
        return MIXER_AddChannel(handler,freq,name);
    } else {
        E_Exit("already added mixer channel.");
        return 0; //Compiler happy
    }
}

MixerObject::~MixerObject(){
    if(!installed) return;
    MIXER_DelChannel(MIXER_FindChannel(m_name));
}

void MENU_mute(bool enabled) {
    mixer.mute=enabled;
    mainMenu.get_item("mixer_mute").check(mixer.mute).refresh_item(mainMenu);
}

bool MENU_get_mute(void) {
    return mixer.mute;
}

void MENU_swapstereo(bool enabled) {
    mixer.swapstereo=enabled;
    mainMenu.get_item("mixer_swapstereo").check(mixer.swapstereo).refresh_item(mainMenu);
}

bool MENU_get_swapstereo(void) {
    return mixer.swapstereo;
}

void MAPPER_VolumeUp(bool pressed) {
    if (!pressed) return;

    double newvol = (((double)mixer.mastervol[0] + mixer.mastervol[1]) / 0.7) * 0.5;

    if (newvol > 1) newvol = 1;

    mixer.mastervol[0] = mixer.mastervol[1] = newvol;

    LOG(LOG_MISC,LOG_NORMAL)("Master volume UP to %.3f%%",newvol * 100);
}

void MAPPER_VolumeDown(bool pressed) {
    if (!pressed) return;

    double newvol = ((double)mixer.mastervol[0] + mixer.mastervol[1]) * 0.7 * 0.5;

    if (fabs(newvol - 1.0) < 0.25)
        newvol = 1;

    mixer.mastervol[0] = mixer.mastervol[1] = newvol;

    LOG(LOG_MISC,LOG_NORMAL)("Master volume DOWN to %.3f%%",newvol * 100);
}

void MAPPER_RecVolumeUp(bool pressed) {
    if (!pressed) return;

    double newvol = (((double)mixer.recordvol[0] + mixer.recordvol[1]) / 0.7) * 0.5;

    if (newvol > 1) newvol = 1;

    mixer.recordvol[0] = mixer.recordvol[1] = newvol;

    LOG(LOG_MISC,LOG_NORMAL)("Recording volume UP to %.3f%%",newvol * 100);
}

void MAPPER_RecVolumeDown(bool pressed) {
    if (!pressed) return;

    double newvol = ((double)mixer.recordvol[0] + mixer.recordvol[1]) * 0.7 * 0.5;

    if (fabs(newvol - 1.0) < 0.25)
        newvol = 1;

    mixer.recordvol[0] = mixer.recordvol[1] = newvol;

    LOG(LOG_MISC,LOG_NORMAL)("Recording volume DOWN to %.3f%%",newvol * 100);
}

void MIXER_Controls_Init() {
    DOSBoxMenu::item *item;

    MAPPER_AddHandler(MAPPER_VolumeUp  ,MK_kpplus, MMODHOST,"volup","VolUp",&item);
    item->set_text("Increase volume");
    
    MAPPER_AddHandler(MAPPER_VolumeDown,MK_kpminus,MMODHOST,"voldown","VolDown",&item);
    item->set_text("Decrease volume");

    MAPPER_AddHandler(MAPPER_RecVolumeUp  ,MK_nothing, 0,"recvolup","RecVolUp",&item);
    item->set_text("Increase recording volume");

    MAPPER_AddHandler(MAPPER_RecVolumeDown,MK_nothing, 0,"recvoldown","RecVolDn",&item);
    item->set_text("Decrease recording volume");
}

void MIXER_DOS_Boot(Section *) {
    PROGRAMS_MakeFile("MIXER.COM",MIXER_ProgramStart);
}

void MIXER_Init() {
    AddExitFunction(AddExitFunctionFuncPair(MIXER_Stop));

    LOG(LOG_MISC,LOG_DEBUG)("Initializing DOSBox audio mixer");

    Section_prop * section=static_cast<Section_prop *>(control->GetSection("mixer"));
    /* Read out config section */
    mixer.freq=(unsigned int)section->Get_int("rate");
    mixer.nosound=section->Get_bool("nosound");
    mixer.blocksize=(unsigned int)section->Get_int("blocksize");
    mixer.swapstereo=section->Get_bool("swapstereo");
    mixer.sampleaccurate=section->Get_bool("sample accurate");
    mixer.mute=false;

    /* Initialize the internal stuff */
    mixer.prebuffer_samples=0;
    mixer.prebuffer_wait=true;
    mixer.channels=0;
    mixer.pos=0;
    mixer.done=0;
    memset(mixer.work,0,sizeof(mixer.work));
    mixer.mastervol[0]=1.0f;
    mixer.mastervol[1]=1.0f;
    mixer.recordvol[0]=1.0f;
    mixer.recordvol[1]=1.0f;

    /* Start the Mixer using SDL Sound at 22 khz */
    SDL_AudioSpec spec;
    SDL_AudioSpec obtained;

    spec.freq=(int)mixer.freq;
    spec.format=AUDIO_S16SYS;
    spec.channels=2;
    spec.callback=MIXER_CallBack;
    spec.userdata=NULL;
    spec.samples=(Uint16)mixer.blocksize;

    if (mixer.nosound) {
        LOG(LOG_MISC,LOG_DEBUG)("MIXER:No Sound Mode Selected.");
        TIMER_AddTickHandler(MIXER_Mix);
    } else if (SDL_OpenAudio(&spec, &obtained) <0 ) {
        mixer.nosound = true;
        LOG(LOG_MISC,LOG_DEBUG)("MIXER:Can't open audio: %s , running in nosound mode.",SDL_GetError());
        TIMER_AddTickHandler(MIXER_Mix);
    } else {
        if(((Bitu)mixer.freq != (Bitu)obtained.freq) || ((Bitu)mixer.blocksize != (Bitu)obtained.samples))
            LOG(LOG_MISC,LOG_DEBUG)("MIXER:Got different values from SDL: freq %d, blocksize %d",(int)obtained.freq,(int)obtained.samples);

        mixer.freq=(unsigned int)obtained.freq;
        mixer.blocksize=obtained.samples;
        TIMER_AddTickHandler(MIXER_Mix);
        if (mixer.sampleaccurate) PIC_AddEvent(MIXER_MixSingle,1000.0 / mixer.freq);
        SDL_PauseAudio(0);
    }
    mixer_start_pic_time = PIC_FullIndex();
    mixer_sample_counter = 0;
    mixer.work_in = mixer.work_out = 0;
    mixer.work_wrap = MIXER_BUFSIZE;
    if (mixer.work_wrap <= mixer.blocksize) E_Exit("blocksize too large");

    {
        int ms = section->Get_int("prebuffer");

        if (ms < 0) ms = 20;

        mixer.prebuffer_samples = ((unsigned int)ms * (unsigned int)mixer.freq) / 1000u;
        if (mixer.prebuffer_samples > (mixer.work_wrap / 2))
            mixer.prebuffer_samples = (mixer.work_wrap / 2);
    }

    // how many samples per millisecond? compute as improper fraction (sample rate / 1000)
    mixer.samples_per_ms.w = mixer.freq / 1000U;
    mixer.samples_per_ms.fn = mixer.freq % 1000U;
    mixer.samples_per_ms.fd = 1000U;
    mixer.samples_this_ms.w = mixer.samples_per_ms.w;
    mixer.samples_this_ms.fn = 0;
    mixer.samples_this_ms.fd = mixer.samples_per_ms.fd;
    mixer.samples_rendered_ms.w = 0;
    mixer.samples_rendered_ms.fn = 0;
    mixer.samples_rendered_ms.fd = mixer.samples_per_ms.fd;

    LOG(LOG_MISC,LOG_DEBUG)("Mixer: sample_accurate=%u blocksize=%u sdl_rate=%uHz mixer_rate=%uHz channels=%u samples=%u min/max/need=%u/%u/%u per_ms=%u %u/%u samples prebuffer=%u",
        (unsigned int)mixer.sampleaccurate,
        (unsigned int)mixer.blocksize,
        (unsigned int)obtained.freq,
        (unsigned int)mixer.freq,
        (unsigned int)obtained.channels,
        (unsigned int)obtained.samples,
        (unsigned int)0,
        (unsigned int)0,
        (unsigned int)0,
        (unsigned int)mixer.samples_per_ms.w,
        (unsigned int)mixer.samples_per_ms.fn,
        (unsigned int)mixer.samples_per_ms.fd,
        (unsigned int)mixer.prebuffer_samples);

    AddVMEventFunction(VM_EVENT_DOS_INIT_KERNEL_READY,AddVMEventFunctionFuncPair(MIXER_DOS_Boot));

    MIXER_Controls_Init();
}

// save state support
//void *MIXER_Mix_NoSound_PIC_Timer = (void*)MIXER_Mix_NoSound;
void *MIXER_Mix_PIC_Timer = (void*)((uintptr_t)MIXER_Mix);


void MixerChannel::SaveState( std::ostream& stream )
{
	// - pure data
	WRITE_POD( &volmain, volmain );
	WRITE_POD( &scale, scale );
	WRITE_POD( &volmul, volmul );
	//WRITE_POD( &freq_add, freq_add );
	WRITE_POD( &enabled, enabled );
}


void MixerChannel::LoadState( std::istream& stream )
{
	// - pure data
	READ_POD( &volmain, volmain );
	READ_POD( &scale, scale );
	READ_POD( &volmul, volmul );
	//READ_POD( &freq_add, freq_add );
	READ_POD( &enabled, enabled );

	//********************************************
	//********************************************
	//********************************************

	// reset mixer channel (system data)
	mixer.pos = 0;
	mixer.done = 0;
}

extern void POD_Save_Adlib(std::ostream& stream);
extern void POD_Save_Disney(std::ostream& stream);
extern void POD_Save_Gameblaster(std::ostream& stream);
extern void POD_Save_GUS(std::ostream& stream);
extern void POD_Save_MPU401(std::ostream& stream);
extern void POD_Save_PCSpeaker(std::ostream& stream);
extern void POD_Save_Sblaster(std::ostream& stream);
extern void POD_Save_Tandy_Sound(std::ostream& stream);
extern void POD_Load_Adlib(std::istream& stream);
extern void POD_Load_Disney(std::istream& stream);
extern void POD_Load_Gameblaster(std::istream& stream);
extern void POD_Load_GUS(std::istream& stream);
extern void POD_Load_MPU401(std::istream& stream);
extern void POD_Load_PCSpeaker(std::istream& stream);
extern void POD_Load_Sblaster(std::istream& stream);
extern void POD_Load_Tandy_Sound(std::istream& stream);

namespace
{
class SerializeMixer : public SerializeGlobalPOD
{
public:
	SerializeMixer() : SerializeGlobalPOD("Mixer")
	{}

private:
	virtual void getBytes(std::ostream& stream)
	{

		//*************************************************
		//*************************************************

		SerializeGlobalPOD::getBytes(stream);


		POD_Save_Adlib(stream);
		POD_Save_Disney(stream);
		POD_Save_Gameblaster(stream);
		POD_Save_GUS(stream);
		POD_Save_MPU401(stream);
		POD_Save_PCSpeaker(stream);
		POD_Save_Sblaster(stream);
		POD_Save_Tandy_Sound(stream);
	}

	virtual void setBytes(std::istream& stream)
	{

		//*************************************************
		//*************************************************

		SerializeGlobalPOD::setBytes(stream);


		POD_Load_Adlib(stream);
		POD_Load_Disney(stream);
		POD_Load_Gameblaster(stream);
		POD_Load_GUS(stream);
		POD_Load_MPU401(stream);
		POD_Load_PCSpeaker(stream);
		POD_Load_Sblaster(stream);
		POD_Load_Tandy_Sound(stream);

		// reset mixer channel (system data)
		mixer.pos = 0;
		mixer.done = 0;
	}

} dummy;
}
