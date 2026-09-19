/*
 *  VESA VBE/AI 1.0 (VESA Audio Interface) provider for DOSBox-X
 *
 *  Answers INT 10h AX=4F13h directly out of the emulated BIOS, the same way
 *  int10_vesa.cpp answers the VESA VBE video calls, so that no driver has to be
 *  loaded in the guest.  Audio is rendered through a dedicated mixer channel; no
 *  I/O port range, IRQ or DMA channel is claimed, because there is no hardware
 *  here to emulate -- VBE/AI is a pure software BIOS interface.
 *
 *  See docs/vbeai.md for the consolidated specification this implements and for
 *  the reasoning behind the design decisions below.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "dosbox.h"
#include "bios.h"
#include "callback.h"
#include "control.h"
#include "cpu.h"
#include "int10.h"
#include "logging.h"
#include "mem.h"
#include "mixer.h"
#include "regs.h"
#include "setup.h"

/* ---------------------------------------------------------------------------
 * Specification constants (VBE/AI 1.0, 02/04/94)
 * ------------------------------------------------------------------------- */

#define VBEAI_VERSION           0x10        /* nibble packed: 1.0 */

/* device classes */
#define VBEAI_CLASS_WAVE        0x0001
#define VBEAI_CLASS_MIDI        0x0002
#define VBEAI_CLASS_VOLUME      0x0003

/* the devices we provide, enumerated in ascending handle order */
#define VBEAI_WAVE_HANDLE       0x0001
#define VBEAI_MIDI_HANDLE       0x0002

/* subfunction 2 general queries */
#define VBEAI_Q_GDC_LENGTH      0x01
#define VBEAI_Q_GDC_COPY        0x02
#define VBEAI_Q_VOLINFO_LENGTH  0x03
#define VBEAI_Q_VOLINFO_COPY    0x04
#define VBEAI_Q_VOLSERV_LENGTH  0x05
#define VBEAI_Q_VOLSERV_COPY    0x06
#define VBEAI_Q_LAST_GENERAL    0x0f

/* WAVE wsDeviceCheck messages */
#define WAVECOMPRESSION         0x0011
#define WAVEDRIVERSTATE         0x0012
#define WAVEGETCURRENTPOS       0x0013
#define WAVESAMPLERATE          0x0014
#define WAVESETPREFERENCE       0x0015
#define WAVEGETDMAIRQ           0x0016
#define WAVEGETIOADDRESS        0x0017
#define WAVEGETMEMADDRESS       0x0018
#define WAVEGETMEMFREE          0x0019
#define WAVEFULLDUPLEX          0x001A
#define WAVEGETBLOCKSIZE        0x001B
#define WAVEGETPCMFORMAT        0x001C
#define WAVEENAPCMFORMAT        0x001D

/* MIDI msDeviceCheck messages */
#define MIDITONES               0x0011
#define MIDIPATCHTYPE           0x0012
#define MIDISETPREFERENCE       0x0013
#define MIDIVOICESTEAL          0x0014
#define MIDIGETFIFOSIZES        0x0015
#define MIDIGETDMAIRQ           0x0016
#define MIDIGETIOADDRESS        0x0017
#define MIDIGETMEMADDRESS       0x0018
#define MIDIGETMEMFREE          0x0019

/* MIDI error codes (msGetLastError) */
#define MID_NOSUPPORT           0x01
#define MID_UNKNOWNPATCH        0x02

/* WAVE error codes (wsGetLastError) */
#define WAV_NOSUPPORT           1
#define WAV_BADSAMPLERATE       2
#define WAV_BADBLOCKLENGTH      3
#define WAV_BADBLOCKADDR        4
#define WAV_BADPCMDATA          6

/* PCM format bits for WAVEGETPCMFORMAT / WAVEENAPCMFORMAT */
#define PCMFMT_8SIGNED          0x00000001
#define PCMFMT_8UNSIGNED        0x00000002
#define PCMFMT_16SIGNED         0x00000010
#define PCMFMT_16UNSIGNED       0x00000020

/* wifeatures: mono+stereo playback at the four standard rates, plus the
 * variable-rate bits.  No record, no WAVEPREPARE (we take the spec's baseline
 * formats -- unsigned 8 bit and signed 16 bit -- directly). */
#define VBEAI_WAVE_FEATURES     0x3004A529UL

/* MIDI mifeatures: a transmitter/receiver with the General MIDI patches already
 * present downstream, which is what DOSBox-X's MIDI output always is.  No input
 * support, so none of the timestamp/interrupt/polled bits.
 *
 * NTS: the spec's prose (5.4) puts "patches preloaded" at 0x40 and internal time
 * stamping at 0x80, leaving 0x20 unassigned.  VBEAI.H *and* VBEAI.INC both put
 * them at 0x20 and 0x40 with no gap.  The headers are what period drivers and
 * applications actually compiled against, so the headers win. */
#define MIDIFXMITR              0x00000010UL    /* transmitter/receiver only  */
#define MIDIFPRELD              0x00000020UL    /* GM patches preloaded       */
#define VBEAI_MIDI_FEATURES     (MIDIFXMITR | MIDIFPRELD)

/* structure sizes */
#define WAVEINFO_LENGTH         126
#define MIDIINFO_LENGTH         138
#define GDC_WAVE_LENGTH         (12 + WAVEINFO_LENGTH)   /* 138 */
#define GDC_MIDI_LENGTH         (12 + MIDIINFO_LENGTH)   /* 150 */
#define WAVESERVICE_LENGTH      84
#define MIDISERVICE_LENGTH      96

/* The services structure is written at offset 0 of the memory block the
 * application donates at open time.  Nothing else goes in there: the
 * real-mode trampolines live in BIOS ROM instead, so an application that
 * clears or reuses its own buffer cannot destroy the driver's code. */
#define VBEAI_SERVICE_OFF       0x0000
#define VBEAI_MEMREQ            0x0080      /* what we report in wimemreq */

#define VBEAI_STUB_SIZE         10          /* MOV AX,imm16 / callback / RETF imm16 */

/* offsets within WAVEService */
#define WS_OFF_NAME             0
#define WS_OFF_LENGTH           4
#define WS_OFF_FUTURE           8
#define WS_OFF_FIRSTFUNC        24
#define WS_OFF_APPLPSYNCCB      76
#define WS_OFF_APPLRSYNCCB      80

/* offsets within MIDIService.  Note the 32-byte mspatches[16] and the 16-byte
 * msfuture[16] before the function table. */
#define MS_OFF_NAME             0
#define MS_OFF_LENGTH           4
#define MS_OFF_PATCHES          8
#define MS_OFF_FUTURE           40
#define MS_OFF_FIRSTFUNC        56
#define MS_OFF_APPLFREECB       88
#define MS_OFF_APPLMIDIIN       92

/* Service function indices.  For each device class the run of indices from the
 * first entry is the order of the pointers in its services structure, so the
 * table can be walked linearly when publishing it. */
enum {
    /* WAVE */
    VF_DEVICECHECK = 0,
    VF_PCMINFO,
    VF_PLAYBLOCK,
    VF_PLAYCONT,
    VF_RECORDBLOCK,
    VF_RECORDCONT,
    VF_PAUSEIO,
    VF_RESUMEIO,
    VF_STOPIO,
    VF_WAVEPREPARE,
    VF_WAVEREGISTER,
    VF_GETLASTERROR,
    VF_TIMERTICK,
    VF_WAVE_COUNT,          /* 13 published WAVE functions */

    /* MIDI */
    VF_MS_DEVICECHECK = VF_WAVE_COUNT,
    VF_MS_GLOBALRESET,
    VF_MS_MIDIMSG,
    VF_MS_POLLMIDI,
    VF_MS_PRELOADPATCH,
    VF_MS_UNLOADPATCH,
    VF_MS_TIMERTICK,
    VF_MS_GETLASTERROR,
    VF_MIDI_END,            /* 8 published MIDI functions */

    VF_SYNCRET = VF_MIDI_END,   /* not published; the callback return address */
    VF_COUNT
};

#define VF_MIDI_COUNT   (VF_MIDI_END - VF_MS_DEVICECHECK)

/* Pascal argument byte counts, i.e. the operand of the stub's RETF. */
static const uint16_t vbeai_func_argbytes[VF_COUNT] = {
    6,      /* wsDeviceCheck   (int, long)                        */
    12,     /* wsPCMInfo       (int, long, int, int, int)         */
    6,      /* wsPlayBlock     (int, long)                        */
    12,     /* wsPlayCont      (void far *, long, long)           */
    6,      /* wsRecordBlock   (int, long)                        */
    12,     /* wsRecordCont    (void far *, long, long)           */
    2,      /* wsPauseIO       (int)                              */
    2,      /* wsResumeIO      (int)                              */
    2,      /* wsStopIO        (int)                              */
    14,     /* wsWavePrepare   (int, int, int, void far *, long)  */
    8,      /* wsWaveRegister  (void huge *, long)                */
    0,      /* wsGetLastError  (void)                             */
    0,      /* wsTimerTick     (void)                             */

    6,      /* msDeviceCheck   (int, long)                        */
    0,      /* msGlobalReset   (void)                             */
    6,      /* msMIDImsg       (char far *, int)                  */
    2,      /* msPollMIDI      (int)                              */
    12,     /* msPreLoadPatch  (int, int, void huge *, long)      */
    4,      /* msUnloadPatch   (int, int)                         */
    0,      /* msTimerTick     (void)                             */
    0,      /* msGetLastError  (void)                             */

    0       /* sync callback return trampoline                    */
};

#define VBEAI_MAX_BLOCKS        32          /* the spec's stated limit */
#define VBEAI_TIMERTICKS        100         /* what we ask the application for */

#define VBEAI_MIN_RATE          4000
#define VBEAI_MAX_RATE          48000

/* ---------------------------------------------------------------------------
 * State
 * ------------------------------------------------------------------------- */

struct VBEAI_Block {
    bool        used;
    RealPt      ptr;        /* as handed to us, for the completion callback */
    PhysPt      addr;       /* linear, so blocks may exceed 64K */
    uint32_t    len;
};

struct VBEAI_Pending {
    RealPt      ptr;
    uint32_t    len;
    bool        record;
};

/* Completion callbacks are produced by the mixer and consumed in guest context,
 * so they queue here in between.  A fixed ring: the depth is bounded by how far
 * behind the guest can get, and if it stops calling wsTimerTick entirely the
 * callbacks are moot anyway. */
#define VBEAI_PENDING_MAX   64

struct VBEAI_PendingQueue {
    VBEAI_Pending   item[VBEAI_PENDING_MAX];
    unsigned int    head = 0;
    unsigned int    count = 0;

    void clear(void) { head = 0; count = 0; }
    bool empty(void) const { return count == 0; }

    void push(const VBEAI_Pending &p) {
        if (count == VBEAI_PENDING_MAX) {       /* drop the oldest */
            head = (head + 1) % VBEAI_PENDING_MAX;
            count--;
        }
        item[(head + count) % VBEAI_PENDING_MAX] = p;
        count++;
    }

    VBEAI_Pending pop(void) {
        VBEAI_Pending p = item[head];
        head = (head + 1) % VBEAI_PENDING_MAX;
        count--;
        return p;
    }
};

static struct {
    bool            enabled = false;
    bool            opened = false;
    uint16_t        memseg = 0;

    /* current PCM stream parameters (wsPCMInfo) */
    uint16_t        channels = 1;
    uint16_t        bits = 8;
    uint32_t        rate = 11025;
    uint32_t        pcmformat = PCMFMT_8UNSIGNED | PCMFMT_16SIGNED;

    int16_t         devpref = 0;
    bool            fullduplex = false;
    uint16_t        lasterror = 0;

    VBEAI_Block     blocks[VBEAI_MAX_BLOCKS + 1];   /* handle 0 is "none" */

    /* playback engine */
    bool            playing = false;
    bool            paused = false;
    bool            continuous = false;
    RealPt          playptr = 0;
    PhysPt          playaddr = 0;
    uint32_t        playlen = 0;
    uint32_t        playpos = 0;
    uint32_t        divlen = 0;
    uint32_t        divdone = 0;
    uint16_t        playhandle = 0;

    VBEAI_PendingQueue pending;

    MixerChannel*   chan = NULL;
} vbeai;

/* MIDI device state.  Much smaller than the WAVE device: we are a MIDI
 * transmitter, so there is no synthesis, no voice allocation and no patch
 * storage here -- messages are handed straight to DOSBox-X's MIDI output. */
static struct {
    bool            opened = false;
    uint16_t        memseg = 0;
    int16_t         devpref = 0;
    uint16_t        lasterror = 0;
    uint32_t        msgbytes = 0;   /* application MIDI bytes forwarded */
} vbeai_midi;

extern void MIDI_RawOutByte(uint8_t data);
extern bool MIDI_Available(void);

static Bitu vbeai_callback = 0;
static bool vbeai_callback_allocated = false;

/* Real-mode entry points, in BIOS ROM: VF_COUNT stubs of VBEAI_STUB_SIZE bytes. */
static RealPt vbeai_stubs = 0;

static inline RealPt VBEAI_StubPtr(uint16_t index) {
    return RealMake(RealSeg(vbeai_stubs),
                    (uint16_t)(RealOff(vbeai_stubs) + index * VBEAI_STUB_SIZE));
}

/* ---------------------------------------------------------------------------
 * Small helpers
 * ------------------------------------------------------------------------- */

static inline unsigned int VBEAI_FrameBytes(void) {
    return (vbeai.bits / 8u) * vbeai.channels;
}

/* Pascal arguments: the last declared parameter sits at SP+4, growing upward
 * through the parameter list in reverse declaration order. */
static inline PhysPt VBEAI_ArgAddr(unsigned int off) {
    return SegPhys(ss) + (PhysPt)((reg_esp + 4u + off) & cpu.stack.mask);
}

static inline uint16_t VBEAI_ArgW(unsigned int off) {
    return mem_readw(VBEAI_ArgAddr(off));
}

static inline uint32_t VBEAI_ArgD(unsigned int off) {
    return mem_readd(VBEAI_ArgAddr(off));
}

static inline void VBEAI_RetW(uint16_t v) {
    reg_ax = v;
}

static inline void VBEAI_RetD(uint32_t v) {
    reg_ax = (uint16_t)(v & 0xffffu);
    reg_dx = (uint16_t)(v >> 16);
}

static void VBEAI_WriteString(PhysPt addr, const char *s, unsigned int field) {
    unsigned int i = 0;
    while (i < field) {
        phys_writeb(addr + i, (uint8_t)(s[i] != 0 ? s[i] : 0));
        if (s[i] == 0) break;
        i++;
    }
    for (; i < field; i++) phys_writeb(addr + i, 0);
}

/* ---------------------------------------------------------------------------
 * Playback engine
 * ------------------------------------------------------------------------- */

static void VBEAI_QueueCallback(RealPt ptr, uint32_t len, bool record) {
    VBEAI_Pending p;
    p.ptr = ptr;
    p.len = len;
    p.record = record;
    vbeai.pending.push(p);
}

static void VBEAI_StopPlayback(bool notify) {
    if (!vbeai.playing) return;

    if (vbeai.chan) {
        vbeai.chan->FillUp();
        vbeai.chan->Enable(false);
    }

    const RealPt ptr = vbeai.playptr;
    const uint32_t len = vbeai.playlen;

    vbeai.playing = false;
    vbeai.paused = false;
    vbeai.continuous = false;
    vbeai.playpos = 0;
    vbeai.divdone = 0;
    vbeai.playhandle = 0;

    /* "All queued buffers are returned to the caller" -- a pre-empted or
     * stopped transfer still hands the buffer back through the sync callback. */
    if (notify) VBEAI_QueueCallback(ptr, len, false);
}

static void VBEAI_ApplyFormat(void) {
    if (!vbeai.chan) return;
    vbeai.chan->SetFreq(vbeai.rate);
}

/* Block playback reached its end: go idle and hand the buffer back.  The spec
 * wants the callback to happen once the data has been heard; handing it to the
 * mixer is the closest thing to "out the DAC" we have. */
static void VBEAI_FinishBlock(void) {
    const RealPt ptr = vbeai.playptr;
    const uint32_t len = vbeai.playlen;

    vbeai.playing = false;
    vbeai.playhandle = 0;
    if (vbeai.chan) vbeai.chan->Enable(false);

    VBEAI_QueueCallback(ptr, len, false);
}

/* Render from guest memory into the mixer.  Runs in the emulator's mixing
 * context, never in guest CPU context, so it must not touch guest registers or
 * the guest stack -- completion callbacks are queued, not delivered, here. */
static void VBEAI_MixerCallback(Bitu len) {
    if (vbeai.chan == NULL) return;

    if (!vbeai.playing || vbeai.paused || len == 0) {
        vbeai.chan->AddSilence();
        return;
    }

    const unsigned int framebytes = VBEAI_FrameBytes();
    if (framebytes == 0) {
        vbeai.chan->AddSilence();
        return;
    }

    static int16_t buf16[1024 * 2];
    static uint8_t buf8[1024 * 2];

    Bitu todo = len;
    unsigned int stall = 0;
    while (todo > 0 && vbeai.playing && !vbeai.paused) {
        uint32_t avail_bytes;

        if (vbeai.continuous) {
            /* circular: the next boundary is either the end of the current
             * division or the wrap point, whichever comes first */
            uint32_t next_stop = vbeai.divlen ?
                ((vbeai.playpos / vbeai.divlen) + 1u) * vbeai.divlen : vbeai.playlen;
            if (next_stop > vbeai.playlen) next_stop = vbeai.playlen;
            avail_bytes = next_stop - vbeai.playpos;
        }
        else {
            avail_bytes = vbeai.playlen - vbeai.playpos;
        }

        Bitu frames = avail_bytes / framebytes;
        if (frames == 0) {
            /* Boundary hit exactly: wrap the ring and carry on.  The stall
             * guard catches a pathological division smaller than one frame,
             * which would otherwise spin here forever. */
            if (vbeai.continuous && ++stall < 64) {
                vbeai.playpos = 0;
                vbeai.divdone = 0;
                continue;
            }
            /* Block mode: fewer than one whole frame left, so nothing further
             * is playable. Finish the block rather than sitting here busy for
             * the rest of time -- a block length that is not a whole number of
             * frames would otherwise never complete and never call back. */
            if (!vbeai.continuous) VBEAI_FinishBlock();
            break;
        }
        stall = 0;

        if (frames > todo) frames = todo;
        if (frames > 1024) frames = 1024;

        const PhysPt src = vbeai.playaddr + (PhysPt)vbeai.playpos;
        const Bitu samples = frames * vbeai.channels;

        if (vbeai.bits == 16) {
            /* read byte-wise so host endianness is irrelevant */
            for (Bitu i = 0; i < samples; i++) {
                const uint8_t lo = mem_readb(src + (PhysPt)(i * 2u));
                const uint8_t hi = mem_readb(src + (PhysPt)(i * 2u + 1u));
                buf16[i] = (int16_t)((uint16_t)lo | ((uint16_t)hi << 8));
            }
            if (vbeai.channels == 2) vbeai.chan->AddSamples_s16(frames, buf16);
            else                     vbeai.chan->AddSamples_m16(frames, buf16);
        }
        else {
            for (Bitu i = 0; i < samples; i++)
                buf8[i] = mem_readb(src + (PhysPt)i);
            if (vbeai.channels == 2) vbeai.chan->AddSamples_s8(frames, buf8);
            else                     vbeai.chan->AddSamples_m8(frames, buf8);
        }

        vbeai.playpos += (uint32_t)(frames * framebytes);
        todo -= frames;

        if (vbeai.continuous) {
            /* Hand back every division as it drains. */
            /* NTS: divdone counts divisions completed within the current lap and
             * is only reset by the wrap below.  Resetting it here would make the
             * condition immediately true again and spin forever. */
            while (vbeai.divlen != 0 &&
                   vbeai.playpos >= (vbeai.divdone + 1u) * vbeai.divlen) {
                const uint32_t off = vbeai.divdone * vbeai.divlen;
                VBEAI_QueueCallback(RealMake(RealSeg(vbeai.playptr),
                                             (uint16_t)(RealOff(vbeai.playptr) + off)),
                                    vbeai.divlen, false);
                vbeai.divdone++;
            }
            if (vbeai.playpos >= vbeai.playlen) {
                vbeai.playpos = 0;
                vbeai.divdone = 0;
            }
        }
        else if (vbeai.playpos >= vbeai.playlen) {
            VBEAI_FinishBlock();
            break;
        }
    }

    if (todo > 0) vbeai.chan->AddSilence();
}

/* ---------------------------------------------------------------------------
 * Completion callback delivery
 *
 * Builds a Pascal frame on the guest stack and lets the trampoline's RETF land
 * in the application's wsApplPSyncCB, with our own sync-return stub as the
 * callback's return address so the queue can be drained in one go.  This is the
 * same technique src/ints/mouse.cpp uses to enter a user mouse handler.
 *
 * Only ever called from wsTimerTick and from the sync-return stub, i.e. from
 * functions that return void -- anywhere else the application's callback would
 * clobber the AX/DX:AX we are about to return.
 * ------------------------------------------------------------------------- */

static bool VBEAI_DeliverPending(void) {
    if (!vbeai.opened || vbeai.pending.empty()) return false;

    const VBEAI_Pending p = vbeai.pending.pop();

    const RealPt cb = p.record ?
        (RealPt)real_readd(vbeai.memseg, WS_OFF_APPLRSYNCCB) :
        (RealPt)real_readd(vbeai.memseg, WS_OFF_APPLPSYNCCB);

    if (cb == 0) return false;      /* application registered no callback */

    const RealPt syncret = VBEAI_StubPtr(VF_SYNCRET);

    /* Pascal: push left to right, so the first declared argument ends up at the
     * highest address.  wsApplPSyncCB(int han, void far *ptr, long len, long) */
    CPU_Push16(VBEAI_WAVE_HANDLE);
    CPU_Push16(RealSeg(p.ptr));
    CPU_Push16(RealOff(p.ptr));
    CPU_Push16((uint16_t)(p.len >> 16));
    CPU_Push16((uint16_t)(p.len & 0xffffu));
    CPU_Push16(0);
    CPU_Push16(0);

    /* return address for the application's callback (it does RETF 14) */
    CPU_Push16(RealSeg(syncret));
    CPU_Push16(RealOff(syncret));

    /* the stub's RETF pops this and enters the application */
    CPU_Push16(RealSeg(cb));
    CPU_Push16(RealOff(cb));

    return true;
}

/* ---------------------------------------------------------------------------
 * wsDeviceCheck -- also reachable, without an open device, through
 * subfunction 2 with DL >= 0x10.  Must therefore not depend on open state.
 * ------------------------------------------------------------------------- */

static uint32_t VBEAI_DeviceCheck(uint16_t msg, uint32_t param) {
    switch (msg) {
    case WAVECOMPRESSION:
        return 0;                       /* no hardware compression */

    case WAVEDRIVERSTATE: {
        if (!vbeai.opened) return 0xffffu;               /* -1, not opened */
        uint16_t st = vbeai.playing ? 1 : 0;
        if (vbeai.paused) st |= 0x80;
        return st;
    }

    case WAVEGETCURRENTPOS:
        return vbeai.playing ? vbeai.playpos : 0;

    case WAVESAMPLERATE:
        if (param < VBEAI_MIN_RATE || param > VBEAI_MAX_RATE) return 0;
        return param;                   /* we can match any rate exactly */

    case WAVESETPREFERENCE: {
        const int16_t old = vbeai.devpref;
        if ((int32_t)param != -1) vbeai.devpref = (int16_t)param;
        return (uint16_t)old;
    }

    case WAVEGETDMAIRQ:
        /* No DMA, no IRQ.  0xFF in every byte means "none". */
        return 0xffffffffUL;

    case WAVEGETIOADDRESS:
        return 0;                       /* no I/O port range */

    case WAVEGETMEMADDRESS:
        return 0xffffffffUL;            /* not memory mapped */

    case WAVEGETMEMFREE:
        return 0;                       /* no on-board memory */

    case WAVEFULLDUPLEX:
        if (param == 0) vbeai.fullduplex = false;
        else if (param == 1) vbeai.fullduplex = false;   /* no record path */
        return vbeai.fullduplex ? 1 : 0;

    case WAVEGETBLOCKSIZE:
        return 1;                       /* single-sample granularity */

    case WAVEGETPCMFORMAT:
        /* LOWORD: supported formats, HIWORD: currently enabled */
        return (PCMFMT_8UNSIGNED | PCMFMT_16SIGNED) |
               ((vbeai.pcmformat & 0xffffu) << 16);

    case WAVEENAPCMFORMAT:
        vbeai.pcmformat = param & (PCMFMT_8UNSIGNED | PCMFMT_16SIGNED);
        return 0;

    default:
        LOG(LOG_MISC, LOG_DEBUG)("VBE/AI: unhandled WAVE device check 0x%04x", (unsigned)msg);
        vbeai.lasterror = WAV_NOSUPPORT;
        return 0;
    }
}

/* ---------------------------------------------------------------------------
 * The service functions themselves
 * ------------------------------------------------------------------------- */

static void VBEAI_Svc_DeviceCheck(void) {
    /* (int msg, long param): SP+4 param_lo, SP+6 param_hi, SP+8 msg */
    const uint32_t param = VBEAI_ArgD(0);
    const uint16_t msg = VBEAI_ArgW(4);
    VBEAI_RetD(VBEAI_DeviceCheck(msg, param));
}

static void VBEAI_Svc_PCMInfo(void) {
    /* (int channels, long rate, int comp, int blocking, int pcmsize) */
    const uint16_t pcmsize  = VBEAI_ArgW(0);
    const uint16_t comp     = VBEAI_ArgW(4);
    const uint32_t rate     = VBEAI_ArgD(6);
    const uint16_t channels = VBEAI_ArgW(10);

    if (comp != 0) { vbeai.lasterror = WAV_NOSUPPORT; VBEAI_RetD(0); return; }
    if (channels != 1 && channels != 2) { vbeai.lasterror = WAV_NOSUPPORT; VBEAI_RetD(0); return; }
    if (pcmsize != 8 && pcmsize != 16) { vbeai.lasterror = WAV_BADPCMDATA; VBEAI_RetD(0); return; }
    if (rate < VBEAI_MIN_RATE || rate > VBEAI_MAX_RATE) { vbeai.lasterror = WAV_BADSAMPLERATE; VBEAI_RetD(0); return; }

    if (vbeai.chan) vbeai.chan->FillUp();

    vbeai.channels = channels;
    vbeai.bits = pcmsize;
    vbeai.rate = rate;
    VBEAI_ApplyFormat();

    LOG(LOG_MISC, LOG_DEBUG)("VBE/AI: wsPCMInfo %uch %ubit %uHz",
        (unsigned)channels, (unsigned)pcmsize, (unsigned)rate);

    VBEAI_RetD(rate);       /* best match -- we take the request exactly */
}

static void VBEAI_Svc_PlayBlock(void) {
    /* (int handle, long reserved) */
    const uint16_t handle = VBEAI_ArgW(4);

    if (handle == 0 || handle > VBEAI_MAX_BLOCKS || !vbeai.blocks[handle].used) {
        vbeai.lasterror = WAV_BADBLOCKADDR;
        VBEAI_RetW(0);
        return;
    }

    /* "if the driver is currently busy with another buffer, it will stop that
     * process, perform the wsApplPSync callback, then proceed" */
    VBEAI_StopPlayback(true);

    const VBEAI_Block &b = vbeai.blocks[handle];
    if (b.len < VBEAI_FrameBytes()) {
        vbeai.lasterror = WAV_BADBLOCKLENGTH;
        VBEAI_RetW(0);
        return;
    }

    vbeai.playptr = b.ptr;
    vbeai.playaddr = b.addr;
    vbeai.playlen = b.len;
    vbeai.playpos = 0;
    vbeai.divlen = 0;
    vbeai.divdone = 0;
    vbeai.continuous = false;
    vbeai.paused = false;
    vbeai.playhandle = handle;
    vbeai.playing = true;

    if (vbeai.chan) {
        VBEAI_ApplyFormat();
        vbeai.chan->Enable(true);
    }

    LOG(LOG_MISC, LOG_DEBUG)("VBE/AI: wsPlayBlock handle %u, %lu bytes",
        (unsigned)handle, (unsigned long)b.len);

    VBEAI_RetW(1);
}

static void VBEAI_Svc_PlayCont(void) {
    /* (void far *buf, long len, long div) */
    const uint32_t div = VBEAI_ArgD(0);
    const uint32_t len = VBEAI_ArgD(4);
    const uint16_t off = VBEAI_ArgW(8);
    const uint16_t seg = VBEAI_ArgW(10);

    if (len == 0 || len > 0x10000UL) { vbeai.lasterror = WAV_BADBLOCKLENGTH; VBEAI_RetW(0); return; }
    if (((uint32_t)off + len) > 0x10000UL) {
        /* "the buffer must conform to the 64k limitations of the XT DMA
         * architecture"; a buffer crossing the page is a legal failure. */
        vbeai.lasterror = WAV_BADBLOCKADDR;
        VBEAI_RetW(0);
        return;
    }
    if (div == 0 || div > len || (len % div) != 0) { vbeai.lasterror = WAV_BADBLOCKLENGTH; VBEAI_RetW(0); return; }

    VBEAI_StopPlayback(true);

    vbeai.playptr = RealMake(seg, off);
    vbeai.playaddr = PhysMake(seg, off);
    vbeai.playlen = len;
    vbeai.playpos = 0;
    vbeai.divlen = div;
    vbeai.divdone = 0;
    vbeai.continuous = true;
    vbeai.paused = false;
    vbeai.playhandle = 0;
    vbeai.playing = true;

    if (vbeai.chan) {
        VBEAI_ApplyFormat();
        vbeai.chan->Enable(true);
    }

    LOG(LOG_MISC, LOG_DEBUG)("VBE/AI: wsPlayCont %04x:%04x len %lu div %lu",
        (unsigned)seg, (unsigned)off, (unsigned long)len, (unsigned long)div);

    VBEAI_RetW(1);
}

static void VBEAI_Svc_RecordUnsupported(void) {
    vbeai.lasterror = WAV_NOSUPPORT;
    VBEAI_RetW(0);
}

static void VBEAI_Svc_PauseIO(void) {
    if (!vbeai.playing || vbeai.paused) { VBEAI_RetW(0); return; }
    if (vbeai.chan) { vbeai.chan->FillUp(); vbeai.chan->Enable(false); }
    vbeai.paused = true;
    VBEAI_RetW(1);
}

static void VBEAI_Svc_ResumeIO(void) {
    if (!vbeai.playing || !vbeai.paused) { VBEAI_RetW(0); return; }
    vbeai.paused = false;
    if (vbeai.chan) vbeai.chan->Enable(true);
    VBEAI_RetW(1);
}

static void VBEAI_Svc_StopIO(void) {
    /* Spec: "Return Value: None." -- so it is safe to leave AX alone. */
    VBEAI_StopPlayback(true);
}

static void VBEAI_Svc_WavePrepare(void) {
    /* We accept the spec's baseline formats as-is, and do not set the
     * WAVEPREPARE feature bit, so there is nothing to convert. */
    VBEAI_RetD(0);
}

static void VBEAI_Svc_WaveRegister(void) {
    /* (void huge *ptr, long len-or-handle) */
    const uint32_t arg = VBEAI_ArgD(0);
    const uint16_t off = VBEAI_ArgW(4);
    const uint16_t seg = VBEAI_ArgW(6);

    if (seg == 0 && off == 0) {
        /* unregister: arg is the handle.  Never fails. */
        const uint16_t handle = (uint16_t)arg;
        if (handle >= 1 && handle <= VBEAI_MAX_BLOCKS) {
            if (vbeai.playing && !vbeai.continuous && vbeai.playhandle == handle)
                VBEAI_StopPlayback(true);
            vbeai.blocks[handle].used = false;
        }
        VBEAI_RetW(0);
        return;
    }

    if (arg == 0) { vbeai.lasterror = WAV_BADBLOCKLENGTH; VBEAI_RetW(0); return; }

    for (uint16_t h = 1; h <= VBEAI_MAX_BLOCKS; h++) {
        if (vbeai.blocks[h].used) continue;
        vbeai.blocks[h].used = true;
        vbeai.blocks[h].ptr = RealMake(seg, off);
        vbeai.blocks[h].addr = PhysMake(seg, off);
        vbeai.blocks[h].len = arg;
        LOG(LOG_MISC, LOG_DEBUG)("VBE/AI: wsWaveRegister %04x:%04x len %lu -> handle %u",
            (unsigned)seg, (unsigned)off, (unsigned long)arg, (unsigned)h);
        VBEAI_RetW(h);
        return;
    }

    vbeai.lasterror = WAV_NOSUPPORT;    /* all 32 slots in use */
    VBEAI_RetW(0);
}

static void VBEAI_Svc_GetLastError(void) {
    const uint16_t e = vbeai.lasterror;
    vbeai.lasterror = 0;                /* "driver will zero out the posted error" */
    VBEAI_RetW(e);
}

static void VBEAI_Svc_TimerTick(void) {
    VBEAI_DeliverPending();
}

/* ---------------------------------------------------------------------------
 * MIDI device
 *
 * We present a MIDI transmitter/receiver (MIDIFXMITR) whose downstream already
 * has the General MIDI patches (MIDIFPRELD), because that is exactly what
 * DOSBox-X's MIDI output is -- whatever the user configured in [midi], be that
 * MT-32, FluidSynth or the host synthesiser.  So there is no synthesis and no
 * patch library here: msMIDImsg hands bytes to MIDI_RawOutByte, which already
 * implements running status, sysex framing and realtime messages.
 * ------------------------------------------------------------------------- */

/* MIDI_RawOutByte dereferences the MIDI handler without checking it, and that
 * handler is null when no MIDI output is configured, so every byte we emit goes
 * through here.  The MIDI device is also not enumerated at all in that case --
 * this is the second line of defence, since availability can change when the
 * user reconfigures [midi] between our enumeration and the guest's call. */
static inline void VBEAI_MidiOut(uint8_t b) {
    if (MIDI_Available()) MIDI_RawOutByte(b);
}

static uint32_t VBEAI_MidiDeviceCheck(uint16_t msg, uint32_t param) {
    switch (msg) {
    case MIDITONES:
        /* "In the case of MIDI devices that do not know the tone count, such as
         * MIDI transmitter/receivers, this function can just return 0xFFFF" */
        return 0xffffu;

    case MIDIPATCHTYPE:
        /* The registered types are OPL2/OPL3 patch formats, which a transmitter
         * does not interpret. */
        return 0;

    case MIDISETPREFERENCE: {
        const int16_t old = vbeai_midi.devpref;
        if ((int32_t)param != -1) vbeai_midi.devpref = (int16_t)param;
        return (uint16_t)old;
    }

    case MIDIVOICESTEAL:
        /* Same guidance as MIDITONES for transmitter/receivers. */
        return 0xffffu;

    case MIDIGETFIFOSIZES:
        return 0;                       /* no physical FIFO either way */

    case MIDIGETDMAIRQ:
        return 0xffffffffUL;            /* no DMA, no IRQ */

    case MIDIGETIOADDRESS:
        return 0;                       /* no I/O port range */

    case MIDIGETMEMADDRESS:
        return 0xffffffffUL;            /* not memory mapped */

    case MIDIGETMEMFREE:
        return 0;                       /* no on-board memory */

    default:
        LOG(LOG_MISC, LOG_DEBUG)("VBE/AI: unhandled MIDI device check 0x%04x", (unsigned)msg);
        vbeai_midi.lasterror = MID_NOSUPPORT;
        return 0;
    }
}

static void VBEAI_Svc_MidiDeviceCheck(void) {
    /* (int msg, long param) */
    const uint32_t param = VBEAI_ArgD(0);
    const uint16_t msg = VBEAI_ArgW(4);
    VBEAI_RetD(VBEAI_MidiDeviceCheck(msg, param));
}

static void VBEAI_Svc_MidiGlobalReset(void) {
    /* "Resets the driver and all voices to an inactive state."  For a
     * transmitter that means silencing the downstream device: All Notes Off and
     * Reset All Controllers on every channel, the same thing mpu401.cpp sends. */
    for (uint8_t ch = 0; ch < 16; ch++) {
        VBEAI_MidiOut((uint8_t)(0xb0 | ch));
        VBEAI_MidiOut(0x7b);          /* All Notes Off          */
        VBEAI_MidiOut(0x00);
        VBEAI_MidiOut((uint8_t)(0xb0 | ch));
        VBEAI_MidiOut(0x79);          /* Reset All Controllers  */
        VBEAI_MidiOut(0x00);
    }
    VBEAI_RetW(1);
}

static void VBEAI_Svc_MidiMsg(void) {
    /* (char far *msgs, int len) -- a block of MIDI bytes, all at delta time 0.
     * The spec requires the driver to behave as if the data arrived one byte at
     * a time, running status included; MIDI_RawOutByte does exactly that. */
    const uint16_t len = VBEAI_ArgW(0);
    const uint16_t off = VBEAI_ArgW(2);
    const uint16_t seg = VBEAI_ArgW(4);

    if (len == 0) { VBEAI_RetW(1); return; }

    const PhysPt src = PhysMake(seg, off);
    for (uint16_t i = 0; i < len; i++)
        VBEAI_MidiOut(mem_readb(src + (PhysPt)i));

    vbeai_midi.msgbytes += len;
    VBEAI_RetW(1);
}

static void VBEAI_Svc_MidiPollMIDI(void) {
    /* MIDI input is not provided (neither MIDIINTR nor MIDIPOLL is advertised),
     * so there is nothing to poll and no msApplMIDIIn callback to make. */
}

static void VBEAI_Svc_MidiPreLoadPatch(void) {
    /* (int patch, int channel, void huge *data, long length)
     *
     * "for MIDI transmitter/receiver devices, the msPreloadPatch function will
     * transmit the whole patch to the external device.  This means the patch
     * data within the library must be in the SYSEX format".  So we forward it
     * verbatim -- but only if it really is sysex, rather than spraying an OPL
     * patch blob at the synthesiser. */
    const uint32_t length = VBEAI_ArgD(0);
    const uint16_t off = VBEAI_ArgW(4);
    const uint16_t seg = VBEAI_ArgW(6);

    if ((seg == 0 && off == 0) || length == 0) {
        vbeai_midi.lasterror = MID_UNKNOWNPATCH;
        VBEAI_RetW(0);
        return;
    }

    const PhysPt src = PhysMake(seg, off);
    if (mem_readb(src) != 0xf0) {       /* not a sysex block */
        vbeai_midi.lasterror = MID_UNKNOWNPATCH;
        VBEAI_RetW(0);
        return;
    }

    for (uint32_t i = 0; i < length; i++)
        VBEAI_MidiOut(mem_readb(src + (PhysPt)i));

    VBEAI_RetW(1);
}

static void VBEAI_Svc_MidiUnloadPatch(void) {
    /* Nothing is retained on this side, so there is nothing to release and no
     * msApplFreeCB to make. */
    VBEAI_RetW(1);
}

static void VBEAI_Svc_MidiTimerTick(void) {
    /* mitimerticks is 0: we need no periodic service. */
}

static void VBEAI_Svc_MidiGetLastError(void) {
    const uint16_t e = vbeai_midi.lasterror;
    vbeai_midi.lasterror = 0;
    VBEAI_RetW(e);
}

/* ---------------------------------------------------------------------------
 * Trampoline dispatch.  One DOSBox-X callback serves every entry point; the
 * stub loads its own index into AX before invoking us.
 * ------------------------------------------------------------------------- */

static Bitu VBEAI_ServiceHandler(void) {
    const uint16_t index = reg_ax;

    /* Services are only live between that device's own open and close. */
    const bool is_midi = (index >= VF_MS_DEVICECHECK && index < VF_MIDI_END);
    const bool live = is_midi ? vbeai_midi.opened : vbeai.opened;

    if (!live && index != VF_SYNCRET) {
        reg_ax = 0;
        reg_dx = 0;
        return CBRET_NONE;
    }

    switch (index) {
    case VF_DEVICECHECK:    VBEAI_Svc_DeviceCheck();        break;
    case VF_PCMINFO:        VBEAI_Svc_PCMInfo();            break;
    case VF_PLAYBLOCK:      VBEAI_Svc_PlayBlock();          break;
    case VF_PLAYCONT:       VBEAI_Svc_PlayCont();           break;
    case VF_RECORDBLOCK:    VBEAI_Svc_RecordUnsupported();  break;
    case VF_RECORDCONT:     VBEAI_Svc_RecordUnsupported();  break;
    case VF_PAUSEIO:        VBEAI_Svc_PauseIO();            break;
    case VF_RESUMEIO:       VBEAI_Svc_ResumeIO();           break;
    case VF_STOPIO:         VBEAI_Svc_StopIO();             break;
    case VF_WAVEPREPARE:    VBEAI_Svc_WavePrepare();        break;
    case VF_WAVEREGISTER:   VBEAI_Svc_WaveRegister();       break;
    case VF_GETLASTERROR:   VBEAI_Svc_GetLastError();       break;
    case VF_TIMERTICK:      VBEAI_Svc_TimerTick();          break;

    case VF_MS_DEVICECHECK:  VBEAI_Svc_MidiDeviceCheck();   break;
    case VF_MS_GLOBALRESET:  VBEAI_Svc_MidiGlobalReset();   break;
    case VF_MS_MIDIMSG:      VBEAI_Svc_MidiMsg();           break;
    case VF_MS_POLLMIDI:     VBEAI_Svc_MidiPollMIDI();      break;
    case VF_MS_PRELOADPATCH: VBEAI_Svc_MidiPreLoadPatch();  break;
    case VF_MS_UNLOADPATCH:  VBEAI_Svc_MidiUnloadPatch();   break;
    case VF_MS_TIMERTICK:    VBEAI_Svc_MidiTimerTick();     break;
    case VF_MS_GETLASTERROR: VBEAI_Svc_MidiGetLastError();  break;
    case VF_SYNCRET:        VBEAI_DeliverPending();         break;
    default:
        LOG(LOG_MISC, LOG_WARN)("VBE/AI: bad service index %u", (unsigned)index);
        break;
    }

    return CBRET_NONE;
}

/* ---------------------------------------------------------------------------
 * Publishing the services structure into the application's block
 * ------------------------------------------------------------------------- */

static void VBEAI_WriteStub(PhysPt at, uint16_t index) {
    phys_writeb(at + 0, 0xB8);                          /* MOV AX,imm16       */
    phys_writew(at + 1, index);
    phys_writeb(at + 3, 0xFE);                          /* DOSBox-X callback  */
    phys_writeb(at + 4, 0x38);
    phys_writew(at + 5, (uint16_t)vbeai_callback);
    phys_writeb(at + 7, 0xCA);                          /* RETF imm16         */
    phys_writew(at + 8, vbeai_func_argbytes[index]);
}

static void VBEAI_PublishServices(uint16_t seg) {
    const PhysPt s = PhysMake(seg, 0) + VBEAI_SERVICE_OFF;

    phys_writeb(s + WS_OFF_NAME + 0, 'W');
    phys_writeb(s + WS_OFF_NAME + 1, 'A');
    phys_writeb(s + WS_OFF_NAME + 2, 'V');
    phys_writeb(s + WS_OFF_NAME + 3, 'S');
    phys_writed(s + WS_OFF_LENGTH, WAVESERVICE_LENGTH);
    for (unsigned int i = 0; i < 16; i++) phys_writeb(s + WS_OFF_FUTURE + i, 0);

    /* the 13 driver-supplied functions, in structure order */
    for (uint16_t i = 0; i < VF_WAVE_COUNT; i++)
        phys_writed(s + WS_OFF_FIRSTFUNC + i * 4, (uint32_t)VBEAI_StubPtr(i));

    /* the application fills these in itself */
    phys_writed(s + WS_OFF_APPLPSYNCCB, 0);
    phys_writed(s + WS_OFF_APPLRSYNCCB, 0);
}

static void VBEAI_PublishMidiServices(uint16_t seg) {
    const PhysPt s = PhysMake(seg, 0) + VBEAI_SERVICE_OFF;

    phys_writeb(s + MS_OFF_NAME + 0, 'M');
    phys_writeb(s + MS_OFF_NAME + 1, 'I');
    phys_writeb(s + MS_OFF_NAME + 2, 'D');
    phys_writeb(s + MS_OFF_NAME + 3, 'S');
    phys_writed(s + MS_OFF_LENGTH, MIDISERVICE_LENGTH);

    /* mspatches[16]: which patches are loaded.  We advertise MIDIFPRELD, so
     * every patch is present and every bit is set -- an application checking
     * this before a program change will never need to send us one. */
    for (unsigned int i = 0; i < 16; i++)
        phys_writew(s + MS_OFF_PATCHES + i * 2, 0xffffu);

    for (unsigned int i = 0; i < 16; i++) phys_writeb(s + MS_OFF_FUTURE + i, 0);

    /* the 8 driver-supplied functions, in structure order */
    for (uint16_t i = 0; i < VF_MIDI_COUNT; i++)
        phys_writed(s + MS_OFF_FIRSTFUNC + i * 4,
                    (uint32_t)VBEAI_StubPtr((uint16_t)(VF_MS_DEVICECHECK + i)));

    /* the application fills these in itself */
    phys_writed(s + MS_OFF_APPLFREECB, 0);
    phys_writed(s + MS_OFF_APPLMIDIIN, 0);
}

static void VBEAI_WriteGeneralDeviceClass(uint16_t seg, uint16_t off) {
    const PhysPt p = PhysMake(seg, off);

    phys_writeb(p + 0, 'V'); phys_writeb(p + 1, 'E');
    phys_writeb(p + 2, 'S'); phys_writeb(p + 3, 'A');
    phys_writed(p + 4, GDC_WAVE_LENGTH);
    phys_writew(p + 8, VBEAI_CLASS_WAVE);
    phys_writew(p + 10, VBEAI_VERSION);

    const PhysPt w = p + 12;                            /* WAVEInfo */
    phys_writeb(w + 0, 'W'); phys_writeb(w + 1, 'A');
    phys_writeb(w + 2, 'V'); phys_writeb(w + 3, 'I');
    phys_writed(w + 4, WAVEINFO_LENGTH);
    phys_writed(w + 8, 0x0100);                         /* wiversion, BCD 1.00 */
    VBEAI_WriteString(w + 12, "DOSBox-X", 32);          /* wivname  */
    VBEAI_WriteString(w + 44, "VBE/AI Provider", 32);   /* wiprod   */
    VBEAI_WriteString(w + 76, "DOSBox-X Mixer", 32);    /* wichip   */
    phys_writeb(w + 108, 0);                            /* wiboardid */
    phys_writeb(w + 109, 0);
    phys_writeb(w + 110, 0);
    phys_writeb(w + 111, 0);
    phys_writed(w + 112, VBEAI_WAVE_FEATURES);
    phys_writew(w + 116, (uint16_t)vbeai.devpref);
    phys_writew(w + 118, VBEAI_MEMREQ);
    phys_writew(w + 120, VBEAI_TIMERTICKS);
    phys_writew(w + 122, 2);                            /* wiChannels: stereo */
    phys_writew(w + 124, 0x0003);                       /* 8 and 16 bit playback */
}

static void VBEAI_WriteMidiDeviceClass(uint16_t seg, uint16_t off) {
    const PhysPt p = PhysMake(seg, off);

    phys_writeb(p + 0, 'V'); phys_writeb(p + 1, 'E');
    phys_writeb(p + 2, 'S'); phys_writeb(p + 3, 'A');
    phys_writed(p + 4, GDC_MIDI_LENGTH);
    phys_writew(p + 8, VBEAI_CLASS_MIDI);
    phys_writew(p + 10, VBEAI_VERSION);

    const PhysPt m = p + 12;                            /* MIDIInfo */
    phys_writeb(m + 0, 'M'); phys_writeb(m + 1, 'I');
    phys_writeb(m + 2, 'D'); phys_writeb(m + 3, 'I');
    phys_writed(m + 4, MIDIINFO_LENGTH);
    phys_writed(m + 8, 0x0100);                         /* miversion, BCD 1.00 */
    VBEAI_WriteString(m + 12, "DOSBox-X", 32);          /* mivname   */
    VBEAI_WriteString(m + 44, "VBE/AI Provider", 32);   /* miprod    */
    VBEAI_WriteString(m + 76, "DOSBox-X MIDI Out", 32); /* michip    */
    phys_writeb(m + 108, 0);                            /* miboardid */
    phys_writeb(m + 109, 0);
    phys_writeb(m + 110, 0);
    phys_writeb(m + 111, 0);
    /* milibrary: empty.  We advertise MIDIFPRELD, so there is no disk-resident
     * patch library for the application to load from. */
    VBEAI_WriteString(m + 112, "", 14);
    phys_writed(m + 126, VBEAI_MIDI_FEATURES);
    phys_writew(m + 130, (uint16_t)vbeai_midi.devpref);
    phys_writew(m + 132, VBEAI_MEMREQ);
    phys_writew(m + 134, 0);                            /* mitimerticks: none  */
    /* miactivetones: unknowable for a transmitter, whose downstream device we
     * cannot interrogate.  0xFFFF is what the spec tells such devices to report
     * for the equivalent MIDITONES device check. */
    phys_writew(m + 136, 0xffffu);
}

/* ---------------------------------------------------------------------------
 * INT 10h AX=4F13h
 * ------------------------------------------------------------------------- */

static inline void VBEAI_Ok(void)   { reg_al = 0x4f; reg_ah = 0x00; }
static inline void VBEAI_Fail(void) { reg_al = 0x4f; reg_ah = 0x01; }

/* Subfunction 2 returns 32-bit results in SI:DI, SI holding the high word. */
static inline void VBEAI_SetSIDI(uint32_t v) {
    reg_si = (uint16_t)(v >> 16);
    reg_di = (uint16_t)(v & 0xffffu);
}

/* "For MIDI Drivers: Turns off all voices.  Frees the DMA, clears any IRQ
 * requests.  Frees any of the applications patch data memory blocks."  We hold
 * no patch memory and no DMA/IRQ, so silencing the device is the whole job. */
static void VBEAI_MidiClose(void) {
    if (vbeai_midi.opened) {
        for (uint8_t ch = 0; ch < 16; ch++) {
            VBEAI_MidiOut((uint8_t)(0xb0 | ch));
            VBEAI_MidiOut(0x7b);                /* All Notes Off */
            VBEAI_MidiOut(0x00);
        }
    }
    if (vbeai_midi.opened)
        LOG(LOG_MISC, LOG_DEBUG)("VBE/AI: MIDI forwarded %lu application bytes",
            (unsigned long)vbeai_midi.msgbytes);

    vbeai_midi.opened = false;
    vbeai_midi.memseg = 0;
    vbeai_midi.lasterror = 0;
    vbeai_midi.msgbytes = 0;
}

static void VBEAI_Close(void) {
    VBEAI_StopPlayback(false);
    vbeai.pending.clear();
    for (unsigned int i = 0; i <= VBEAI_MAX_BLOCKS; i++) vbeai.blocks[i].used = false;
    vbeai.opened = false;
    vbeai.memseg = 0;
    if (vbeai.chan) vbeai.chan->Enable(false);
}

/* Is this handle one of ours, and is the device actually present?  The MIDI
 * device exists only when DOSBox-X has a MIDI output to hand it to. */
static bool VBEAI_HandleLive(uint16_t handle) {
    if (handle == VBEAI_WAVE_HANDLE) return true;
    if (handle == VBEAI_MIDI_HANDLE) return MIDI_Available();
    return false;
}

/* Enumeration walks our devices in ascending handle order, returning the first
 * one past `prev` that matches the requested class (0 meaning any).  Zero means
 * there are no more, which is what ends the application's loop. */
static uint16_t VBEAI_NextHandle(uint16_t prev, uint8_t cls) {
    static const struct { uint16_t handle; uint8_t cls; } devs[] = {
        { VBEAI_WAVE_HANDLE, VBEAI_CLASS_WAVE },
        { VBEAI_MIDI_HANDLE, VBEAI_CLASS_MIDI }
    };

    for (unsigned int i = 0; i < (sizeof(devs)/sizeof(devs[0])); i++) {
        if (devs[i].handle <= prev) continue;
        if (cls != 0 && cls != devs[i].cls) continue;
        if (!VBEAI_HandleLive(devs[i].handle)) continue;
        return devs[i].handle;
    }
    return 0;
}

bool INT10_VBEAI_Handler(void) {
    if (!vbeai.enabled) return false;

    switch (reg_bl) {
    case 0x00:      /* Driver Check */
        if (reg_bh != 0) { VBEAI_Fail(); break; }
        VBEAI_Ok();
        reg_bl = VBEAI_VERSION;
        break;

    case 0x01: {    /* Get Next Device Handle */
        if (reg_bh != 0) { VBEAI_Fail(); break; }
        reg_cx = VBEAI_NextHandle(reg_cx, reg_dl);
        VBEAI_Ok();
        break;
    }

    case 0x02: {    /* Query Device Class Info */
        if (reg_bh != 0) { VBEAI_Fail(); break; }
        if (!VBEAI_HandleLive(reg_cx)) { VBEAI_Fail(); break; }
        if (reg_dh != 0) { VBEAI_Fail(); break; }   /* 32-bit flat not supported */

        const bool midi = (reg_cx == VBEAI_MIDI_HANDLE);
        const uint8_t query = reg_dl;

        if (query >= 0x10) {
            /* device check; SI:DI in, SI:DI out */
            const uint32_t param = ((uint32_t)reg_si << 16) | (uint32_t)reg_di;
            VBEAI_SetSIDI(midi ? VBEAI_MidiDeviceCheck(query, param)
                               : VBEAI_DeviceCheck(query, param));
            VBEAI_Ok();
            break;
        }

        switch (query) {
        case VBEAI_Q_GDC_LENGTH:
            VBEAI_SetSIDI(midi ? GDC_MIDI_LENGTH : GDC_WAVE_LENGTH);
            VBEAI_Ok();
            break;

        case VBEAI_Q_GDC_COPY:
            /* SI:DI is the caller's buffer and must come back unchanged --
             * VESA.C casts it straight to fpGDC after the call. */
            if (midi) VBEAI_WriteMidiDeviceClass(reg_si, reg_di);
            else      VBEAI_WriteGeneralDeviceClass(reg_si, reg_di);
            VBEAI_Ok();
            break;

        case VBEAI_Q_VOLINFO_LENGTH:
        case VBEAI_Q_VOLSERV_LENGTH:
            /* "DX is NULL if the device class does not have a volume control" */
            VBEAI_SetSIDI(0);
            reg_dx = 0;
            VBEAI_Ok();
            break;

        case VBEAI_Q_VOLINFO_COPY:
        case VBEAI_Q_VOLSERV_COPY:
            reg_dx = 0;
            VBEAI_Fail();
            break;

        default:
            VBEAI_Fail();
            break;
        }
        break;
    }

    case 0x03: {    /* Open Device */
        if (reg_bh != 0) { VBEAI_Fail(); break; }
        if (!VBEAI_HandleLive(reg_cx)) { VBEAI_Fail(); break; }
        if (reg_dx != 0) { VBEAI_Fail(); break; }   /* 32-bit interface */

        if (reg_cx == VBEAI_MIDI_HANDLE) {
            if (vbeai_midi.opened) {
                reg_si = 0; reg_cx = 0;     /* already in use */
                VBEAI_Ok();
                break;
            }

            const uint16_t mseg = reg_si;
            if (mseg == 0) { VBEAI_Fail(); break; }

            vbeai_midi.memseg = mseg;
            vbeai_midi.opened = true;
            vbeai_midi.lasterror = 0;
            VBEAI_PublishMidiServices(mseg);

            LOG(LOG_MISC, LOG_DEBUG)("VBE/AI: MIDI device opened, block at %04x:0000", (unsigned)mseg);

            reg_si = mseg;
            reg_cx = VBEAI_SERVICE_OFF;
            VBEAI_Ok();
            break;
        }

        if (vbeai.opened) {
            /* "The driver will return a zero if the requested API is not
             * available (maybe already in use)." */
            reg_si = 0;
            reg_cx = 0;
            VBEAI_Ok();
            break;
        }

        const uint16_t seg = reg_si;
        if (seg == 0) { VBEAI_Fail(); break; }

        vbeai.memseg = seg;
        vbeai.opened = true;
        vbeai.lasterror = 0;
        vbeai.pending.clear();
        for (unsigned int i = 0; i <= VBEAI_MAX_BLOCKS; i++) vbeai.blocks[i].used = false;

        VBEAI_PublishServices(seg);
        VBEAI_ApplyFormat();

        LOG(LOG_MISC, LOG_DEBUG)("VBE/AI: WAVE device opened, block at %04x:0000", (unsigned)seg);

        reg_si = seg;           /* SI:CX = far pointer to the services table */
        reg_cx = VBEAI_SERVICE_OFF;
        VBEAI_Ok();
        break;
    }

    case 0x04:      /* Close Device */
        if (reg_bh != 0) { VBEAI_Fail(); break; }
        if (!VBEAI_HandleLive(reg_cx)) { VBEAI_Fail(); break; }
        if (reg_cx == VBEAI_MIDI_HANDLE) {
            VBEAI_MidiClose();
            LOG(LOG_MISC, LOG_DEBUG)("VBE/AI: MIDI device closed");
        }
        else {
            VBEAI_Close();
            LOG(LOG_MISC, LOG_DEBUG)("VBE/AI: WAVE device closed");
        }
        VBEAI_Ok();
        break;

    case 0x05:      /* Driver Unload Request */
        /* We are part of the emulated BIOS and can never unload. */
        VBEAI_Fail();
        break;

    case 0x06:
        /* Driver chaining, for real TSR drivers only.  Failing step 1 is an
         * explicitly legal answer: the caller then hooks INT 10h directly and
         * becomes a permanent member of the chain. */
        VBEAI_Fail();
        break;

    case 0x07:      /* 32-bit interface loading -- undefined in the 1.0 spec */
        VBEAI_Fail();
        break;

    default:
        LOG(LOG_MISC, LOG_DEBUG)("VBE/AI: unhandled subfunction BX=%04x", (unsigned)reg_bx);
        VBEAI_Fail();
        break;
    }

    return true;
}

/* ---------------------------------------------------------------------------
 * Setup / teardown
 * ------------------------------------------------------------------------- */

bool VBEAI_IsEnabled(void) {
    return vbeai.enabled;
}

void VBEAI_ShutDown(void) {
    VBEAI_MidiClose();
    VBEAI_Close();
    vbeai.enabled = false;
}

/* Called from INT10_Startup(), i.e. every time the emulated INT 10h BIOS is
 * (re)built.  Callback numbers are allocated once and reused; the trampolines
 * themselves live in the application's block and are rewritten at every open. */
void VBEAI_Setup(void) {
    Section_prop *section = static_cast<Section_prop *>(control->GetSection("vbeai"));
    const bool enable = (section != NULL) ? section->Get_bool("vbeai") : false;

    VBEAI_MidiClose();
    VBEAI_Close();
    vbeai.enabled = enable;

    if (!enable) {
        if (vbeai.chan) { vbeai.chan->Enable(false); }
        LOG(LOG_MISC, LOG_DEBUG)("VBE/AI: disabled");
        return;
    }

    if (!vbeai_callback_allocated) {
        vbeai_callback = CALLBACK_Allocate();
        vbeai_callback_allocated = true;
    }
    /* Register the handler without emitting a stub of its own -- every entry
     * point we publish is a stub we write by hand, because the Pascal calling
     * convention needs a per-function RETF imm16 that CB_RETF cannot express. */
    CALLBACK_Setup(vbeai_callback, &VBEAI_ServiceHandler, CB_RETN, "VBE/AI services");

    /* Put the entry points in BIOS ROM, where a VBE/AI driver would live on a
     * real machine.  Reallocated on each INT 10h (re)init, alongside the rest
     * of the BIOS. */
    {
        const Bitu base = ROMBIOS_GetMemory(VF_COUNT * VBEAI_STUB_SIZE,
                                            "VBE/AI service entry points", 1, 0);
        if (base == 0) {
            LOG(LOG_MISC, LOG_WARN)("VBE/AI: no ROM BIOS space for entry points, disabling");
            vbeai.enabled = false;
            return;
        }
        vbeai_stubs = RealMake((uint16_t)(base >> 4), (uint16_t)(base & 0x0f));
        for (uint16_t i = 0; i < VF_COUNT; i++)
            VBEAI_WriteStub((PhysPt)(base + i * VBEAI_STUB_SIZE), i);
    }

    if (vbeai.chan == NULL) {
        /* Created once and kept for the lifetime of the process: the channel is
         * cheap when disabled, and this avoids any static-destruction ordering
         * question against the mixer itself. */
        vbeai.chan = MIXER_AddChannel(&VBEAI_MixerCallback, vbeai.rate, "VBEAI");
        if (vbeai.chan == NULL)
            LOG(LOG_MISC, LOG_WARN)("VBE/AI: could not create mixer channel");
    }
    if (vbeai.chan) vbeai.chan->Enable(false);

    LOG(LOG_MISC, LOG_DEBUG)("VBE/AI: enabled, WAVE device (handle %u)%s",
        VBEAI_WAVE_HANDLE,
        MIDI_Available() ? ", MIDI device (handle 2)" : ", no MIDI output so no MIDI device");
}
