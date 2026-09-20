/*
 *  vbeaiwav -- minimal VESA VBE/AI playback test for DOSBox-X
 *
 *  Plays a RIFF/WAVE file through the VBE/AI WAVE device, a Standard MIDI
 *  File through the VBE/AI MIDI device, or both at once, all via INT 10h
 *  AX=4F13h.  Each file's magic decides which device it goes to.  Its only
 *  purpose is to prove the DOSBox-X built-in VBE/AI provider end to end; it
 *  is deliberately not a general-purpose player.
 *
 *  Note that VBE/AI leaves tempo and scheduling to the application -- the
 *  driver is only ever handed events that are already due -- so the MIDI path
 *  carries a small sequencer of its own.  Both devices are therefore driven
 *  from one loop in which neither call blocks: wave_poll() services the
 *  driver's timer tick, midi_poll() releases whatever has fallen due.
 *
 *  Build with Open Watcom (16-bit real mode, large model):
 *
 *      wcl -0 -ml -bcl=dos -fe=vbeaiwav.exe vbeaiwav.c
 *
 *  Usage:  VBEAIWAV [file.wav] [file.mid]        (defaults to TEST.WAV)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <conio.h>
#include <i86.h>
#include <io.h>
#include <fcntl.h>

/* ------------------------------------------------------------------ */
/* VBE/AI, as far as this test needs it                               */
/* ------------------------------------------------------------------ */

#define VESAFUNCID      0x4f13
#define VESAOK          0x004f

#define VF_DRIVERCHECK  0x0000
#define VF_LOCATE       0x0001
#define VF_QUERY        0x0002
#define VF_OPEN         0x0003
#define VF_CLOSE        0x0004

#define WAVDEVICE       0x0001

#define Q_GDC_COPY      0x0002
#define WAVEDRIVERSTATE 0x0012

#pragma pack(push, 1)

typedef struct {
    char            winame[4];
    long            wilength;
    long            wiversion;
    char            wivname[32];
    char            wiprod[32];
    char            wichip[32];
    char            wiboardid;
    char            wiunused[3];
    long            wifeatures;
    int             widevpref;
    int             wimemreq;
    int             witimerticks;
    int             wiChannels;
    int             wiSampleSize;
} WAVEInfo;

typedef struct {
    char            gdname[4];
    long            gdlength;
    int             gdclassid;
    int             gdvbever;
    WAVEInfo        wi;                 /* the union, WAVE arm */
} GeneralDeviceClass;

typedef struct {
    char            wsname[4];
    long            wslength;
    char            wsfuture[16];

    long (__pascal __far *wsDeviceCheck )(int, long);
    long (__pascal __far *wsPCMInfo     )(int, long, int, int, int);
    int  (__pascal __far *wsPlayBlock   )(int, long);
    int  (__pascal __far *wsPlayCont    )(void __far *, long, long);
    int  (__pascal __far *wsRecordBlock )(int, long);
    int  (__pascal __far *wsRecordCont  )(void __far *, long, long);
    int  (__pascal __far *wsPauseIO     )(int);
    int  (__pascal __far *wsResumeIO    )(int);
    int  (__pascal __far *wsStopIO      )(int);
    int  (__pascal __far *wsWavePrepare )(int, int, int, void __far *, long);
    int  (__pascal __far *wsWaveRegister)(void __far *, long);
    int  (__pascal __far *wsGetLastError)(void);
    void (__pascal __far *wsTimerTick   )(void);

    void (__pascal __far *wsApplPSyncCB )(int, void __far *, long, long);
    void (__pascal __far *wsApplRSyncCB )(int, void __far *, long, long);
} WAVEService;

typedef struct {
    char            miname[4];
    long            milength;
    long            miversion;
    char            mivname[32];
    char            miprod[32];
    char            michip[32];
    char            miboardid;
    char            miunused[3];
    char            milibrary[14];
    long            mifeatures;
    int             midevpref;
    int             mimemreq;
    int             mitimerticks;
    int             miactivetones;
} MIDIInfo;

typedef struct {
    char            gdname[4];
    long            gdlength;
    int             gdclassid;
    int             gdvbever;
    MIDIInfo        mi;
} MidiDeviceClass;

typedef struct {
    char            msname[4];
    long            mslength;
    int             mspatches[16];
    char            msfuture[16];

    long (__pascal __far *msDeviceCheck  )(int, long);
    int  (__pascal __far *msGlobalReset  )(void);
    int  (__pascal __far *msMIDImsg      )(char __far *, int);
    void (__pascal __far *msPollMIDI     )(int);
    int  (__pascal __far *msPreLoadPatch )(int, int, void __far *, long);
    int  (__pascal __far *msUnloadPatch  )(int, int);
    void (__pascal __far *msTimerTick    )(void);
    int  (__pascal __far *msGetLastError )(void);

    void (__pascal __far *msApplFreeCB   )(int, int, void __far *, long);
    void (__pascal __far *msApplMIDIIn   )(int, int, char, long);
} MIDIService;

#pragma pack(pop)

#define MIDDEVICE       0x0002

/* ------------------------------------------------------------------ */

static volatile int     playback_done = 0;
static WAVEService __far *ws = 0;
static int              hWave = 0;
static unsigned         memblock_seg = 0;
static unsigned         data_seg = 0;
static int              block_handle = 0;

/* Called by the driver when the block has finished playing.  __loadds because
 * the spec is explicit that no assumption may be made about the segment
 * registers on entry. */
static void __pascal __far __loadds PlayDone(int han, void __far *ptr,
                                             long len, long reserved)
{
    (void)han; (void)ptr; (void)len; (void)reserved;
    playback_done = 1;
}

/* ------------------------------------------------------------------ */
/* WAV parsing -- just enough to find the format and the samples      */
/* ------------------------------------------------------------------ */

typedef struct {
    int             channels;
    long            rate;
    int             bits;
    long            data_off;
    unsigned long   data_len;
} WavInfo;

static int read_wav_header(FILE *fp, WavInfo *wi)
{
    unsigned char hdr[12];
    unsigned char ck[8];
    unsigned char fmt[16];
    unsigned long size;
    int got_fmt = 0;

    if (fread(hdr, 1, 12, fp) != 12) return 0;
    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
        printf("Not a RIFF/WAVE file.\n");
        return 0;
    }

    for (;;) {
        if (fread(ck, 1, 8, fp) != 8) break;
        size = (unsigned long)ck[4] | ((unsigned long)ck[5] << 8) |
               ((unsigned long)ck[6] << 16) | ((unsigned long)ck[7] << 24);

        if (memcmp(ck, "fmt ", 4) == 0) {
            if (size < 16 || fread(fmt, 1, 16, fp) != 16) return 0;
            if ((fmt[0] | (fmt[1] << 8)) != 1) {
                printf("Only uncompressed PCM is supported.\n");
                return 0;
            }
            wi->channels = fmt[2] | (fmt[3] << 8);
            wi->rate     = (long)fmt[4] | ((long)fmt[5] << 8) |
                           ((long)fmt[6] << 16) | ((long)fmt[7] << 24);
            wi->bits     = fmt[14] | (fmt[15] << 8);
            got_fmt = 1;
            if (size > 16) fseek(fp, (long)(size - 16), SEEK_CUR);
        }
        else if (memcmp(ck, "data", 4) == 0) {
            wi->data_off = ftell(fp);
            wi->data_len = size;
            return got_fmt;
        }
        else {
            fseek(fp, (long)size, SEEK_CUR);
        }
        if (size & 1) fseek(fp, 1L, SEEK_CUR);   /* chunks are word aligned */
    }

    return 0;
}

/* ------------------------------------------------------------------ */

static void cleanup(void)
{
    if (ws != 0) {
        if (block_handle != 0) {
            ws->wsStopIO(0);
            ws->wsWaveRegister((void __far *)0, (long)block_handle);
            block_handle = 0;
        }
        ws = 0;
    }
    if (hWave != 0) {
        union REGS r;
        r.w.ax = VESAFUNCID;
        r.w.bx = VF_CLOSE;
        r.w.cx = (unsigned)hWave;
        int86(0x10, &r, &r);
        hWave = 0;
    }
    if (memblock_seg) { _dos_freemem(memblock_seg); memblock_seg = 0; }
    if (data_seg)     { _dos_freemem(data_seg);     data_seg = 0; }
}

/* ------------------------------------------------------------------ */
/* Standard MIDI File playback                                        */
/*                                                                    */
/* VBE/AI puts tempo and scheduling on the application: the driver is */
/* handed events that are already at delta time zero. So this is a    */
/* small sequencer -- read the file, merge the tracks, and feed each  */
/* event to msMIDImsg at the right moment.                            */
/* ------------------------------------------------------------------ */

#define MAX_TRACKS      32
#define MIDI_BUFSEG_MAX 0xFFFFUL

typedef struct {
    unsigned long   pos;        /* read cursor, offset into the file image */
    unsigned long   end;        /* one past this track's last byte         */
    unsigned long   nexttick;   /* absolute tick of its pending event      */
    unsigned char   running;    /* running status byte                     */
    int             done;
} Track;

static unsigned      mid_seg = 0;       /* segment holding the whole file */
static Track         tracks[MAX_TRACKS];
static int           ntracks = 0;
static unsigned long tempo_us = 500000UL;   /* SMF default: 120 bpm */
static unsigned      division = 96;

/* The schedule is accumulated rather than recomputed from tick 0, for two
 * reasons: a tempo change must only affect the music after it, not retime
 * everything before it; and tick*us_per_tick would overflow 32 bits on a long
 * file. The split multiply keeps the intermediate in range while staying exact. */
static unsigned long cur_tick = 0;
static unsigned long cur_us   = 0;

static unsigned long advance_to(unsigned long tick)
{
    unsigned long d = tick - cur_tick;

    cur_us  += (d / division) * tempo_us + ((d % division) * tempo_us) / division;
    cur_tick = tick;
    return cur_us;
}

static unsigned char mbyte(unsigned long off)
{
    return *(unsigned char __far *)MK_FP((unsigned)(mid_seg + (unsigned)(off >> 4)),
                                         (unsigned)(off & 0x0FUL));
}

static unsigned long mbe32(unsigned long off)
{
    return ((unsigned long)mbyte(off)   << 24) | ((unsigned long)mbyte(off+1) << 16) |
           ((unsigned long)mbyte(off+2) <<  8) |  (unsigned long)mbyte(off+3);
}

static unsigned mbe16(unsigned long off)
{
    return ((unsigned)mbyte(off) << 8) | mbyte(off+1);
}

/* Variable-length quantity: 7 bits per byte, high bit means "continues". */
static unsigned long read_vlq(unsigned long *p)
{
    unsigned long v = 0;
    unsigned char b;
    int guard = 0;

    do {
        b = mbyte((*p)++);
        v = (v << 7) | (unsigned long)(b & 0x7F);
    } while ((b & 0x80) && ++guard < 4);

    return v;
}

/* A free-running microsecond clock. The BIOS tick alone is 55ms, far too
 * coarse for music, so combine it with the PIT channel 0 counter. */
static unsigned long base_ticks = 0;

static unsigned long now_us(void)
{
    unsigned long t1, t2;
    unsigned int cnt;

    do {
        t1 = *(unsigned long __far *)MK_FP(0x40, 0x6C);
        _disable();
        outp(0x43, 0x00);                       /* latch counter 0 */
        cnt  = (unsigned int)inp(0x40);
        cnt |= (unsigned int)inp(0x40) << 8;
        _enable();
        t2 = *(unsigned long __far *)MK_FP(0x40, 0x6C);
    } while (t1 != t2);                          /* retry across a tick edge */

    /* counter 0 counts down from 65536 across one 54925us tick */
    return (t1 - base_ticks) * 54925UL
         + (((65536UL - (unsigned long)cnt) * 54925UL) >> 16);
}

static int parse_smf(unsigned long size)
{
    unsigned long off;
    int i;

    if (size < 14 || mbe32(0) != 0x4D546864UL) {     /* 'MThd' */
        printf("Not a Standard MIDI File.\n");
        return 0;
    }

    division = mbe16(12);
    if (division & 0x8000) {
        printf("SMPTE timing is not supported.\n");
        return 0;
    }
    if (division == 0) division = 96;

    printf("format %u, %u track(s), %u ticks/quarter\n",
           mbe16(8), mbe16(10), division);

    off = 8 + mbe32(4);                              /* past the MThd chunk */
    ntracks = 0;
    while (off + 8 <= size && ntracks < MAX_TRACKS) {
        unsigned long id  = mbe32(off);
        unsigned long len = mbe32(off + 4);
        if (id == 0x4D54726BUL) {                    /* 'MTrk' */
            tracks[ntracks].pos     = off + 8;
            tracks[ntracks].end     = off + 8 + len;
            if (tracks[ntracks].end > size) tracks[ntracks].end = size;
            tracks[ntracks].running = 0;
            tracks[ntracks].done    = 0;
            tracks[ntracks].nexttick = 0;
            ntracks++;
        }
        off += 8 + len;
    }

    if (ntracks == 0) { printf("No MTrk chunks.\n"); return 0; }

    /* prime each track with its first delta time */
    for (i = 0; i < ntracks; i++) {
        if (tracks[i].pos >= tracks[i].end) { tracks[i].done = 1; continue; }
        tracks[i].nexttick = read_vlq(&tracks[i].pos);
    }
    return 1;
}

/* Emit one event from track t, which is due now. Returns 0 at end of track. */
static int play_event(MIDIService __far *ms, Track *t)
{
    static char __far *msgbuf = 0;
    static char buf[4];
    unsigned char status;
    unsigned long len;
    int n;

    if (t->pos >= t->end) { t->done = 1; return 0; }

    status = mbyte(t->pos);
    if (status & 0x80) t->pos++;
    else               status = t->running;      /* running status */

    if (status == 0xFF) {                        /* meta event */
        unsigned char type = mbyte(t->pos++);
        len = read_vlq(&t->pos);
        if (type == 0x2F) { t->done = 1; return 0; }        /* end of track */
        if (type == 0x51 && len == 3) {                     /* set tempo */
            tempo_us = ((unsigned long)mbyte(t->pos)   << 16) |
                       ((unsigned long)mbyte(t->pos+1) <<  8) |
                        (unsigned long)mbyte(t->pos+2);
            if (tempo_us == 0) tempo_us = 500000UL;
        }
        t->pos += len;
        return 1;
    }

    if (status == 0xF0 || status == 0xF7) {      /* sysex */
        len = read_vlq(&t->pos);
        /* Hand it over one chunk at a time through the app's own buffer; the
         * driver forwards raw bytes, so reconstruct the leading F0. */
        msgbuf = (char __far *)MK_FP((unsigned)(mid_seg + (unsigned)(t->pos >> 4)),
                                     (unsigned)(t->pos & 0x0FUL));
        if (status == 0xF0) { buf[0] = (char)0xF0; (ms->msMIDImsg)(buf, 1); }
        if (len > 0 && len < 0x4000UL) (ms->msMIDImsg)(msgbuf, (int)len);
        t->pos += len;
        return 1;
    }

    /* channel voice message */
    t->running = status;
    n = ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) ? 1 : 2;
    buf[0] = (char)status;
    buf[1] = (char)mbyte(t->pos++);
    if (n == 2) buf[2] = (char)mbyte(t->pos++);
    (ms->msMIDImsg)(buf, n + 1);
    return 1;
}

static MIDIService __far *ms = 0;
static unsigned      midi_memseg = 0;
static int           hMIDI = 0;
static unsigned long midi_start = 0;

/* The tick the sequencer is waiting on, and the moment it falls due. Held
 * across calls because midi_poll() returns to its caller while waiting, and
 * advance_to() may be called only once per tick -- it accumulates. */
static unsigned long midi_due_tick = 0;
static unsigned long midi_due_us   = 0;
static int           midi_due_valid = 0;

static int midi_setup(const char *path)
{
    union REGS r;
    MidiDeviceClass gdc;
    int fh;
    unsigned long size, done;
    long fsize;

    /* --- locate a MIDI device --- */
    r.w.ax = VESAFUNCID;
    r.w.bx = VF_LOCATE;
    r.w.cx = 0;
    r.w.dx = MIDDEVICE;
    int86(0x10, &r, &r);
    if (r.w.ax != VESAOK || r.w.cx == 0) {
        printf("No VBE/AI MIDI device found.\n");
        printf("Is a MIDI output configured in the [midi] section?\n");
        return 1;
    }
    hMIDI = (int)r.w.cx;

    /* --- query it --- */
    r.w.ax = VESAFUNCID;
    r.w.bx = VF_QUERY;
    r.w.cx = (unsigned)hMIDI;
    r.w.dx = Q_GDC_COPY;
    r.w.si = FP_SEG((void __far *)&gdc);
    r.w.di = FP_OFF((void __far *)&gdc);
    int86(0x10, &r, &r);
    if (r.w.ax != VESAOK) { printf("MIDI query failed.\n"); return 1; }

    printf("MIDI:   %s / %s (%s)\n", gdc.mi.mivname, gdc.mi.miprod, gdc.mi.michip);
    printf("        features=%08lX memreq=%u tones=%u\n",
           gdc.mi.mifeatures, (unsigned)gdc.mi.mimemreq,
           (unsigned)gdc.mi.miactivetones);

    /* --- load the file --- */
    if (_dos_open(path, O_RDONLY, &fh) != 0) {
        printf("Cannot open %s\n", path); return 1;
    }
    fsize = lseek(fh, 0L, SEEK_END);
    if (fsize <= 0) { printf("Empty file.\n"); _dos_close(fh); return 1; }
    size = (unsigned long)fsize;
    lseek(fh, 0L, SEEK_SET);

    if (_dos_allocmem((unsigned)((size + 15UL) / 16UL), &mid_seg) != 0) {
        printf("Out of memory for %lu bytes.\n", size);
        _dos_close(fh); return 1;
    }

    done = 0;
    while (done < size) {
        unsigned long left = size - done;
        unsigned chunk = (left > 0x8000UL) ? 0x8000u : (unsigned)left;
        unsigned got = 0;
        void __far *dst = MK_FP((unsigned)(mid_seg + (unsigned)(done >> 4)),
                                (unsigned)(done & 0x0FUL));
        if (_dos_read(fh, dst, chunk, &got) != 0 || got == 0) break;
        done += got;
    }
    _dos_close(fh);
    size = done;

    printf("%s: %lu bytes, ", path, size);
    if (!parse_smf(size)) return 1;

    /* --- open the device --- */
    if (_dos_allocmem((unsigned)((gdc.mi.mimemreq + 15) / 16), &midi_memseg) != 0) {
        printf("Out of memory for the driver block.\n");
        return 1;
    }

    r.w.ax = VESAFUNCID;
    r.w.bx = VF_OPEN;
    r.w.cx = (unsigned)hMIDI;
    r.w.dx = 0;
    r.w.si = midi_memseg;
    int86(0x10, &r, &r);
    if (r.w.ax != VESAOK || (r.w.si == 0 && r.w.cx == 0)) {
        printf("MIDI open failed.\n");
        return 1;
    }
    ms = (MIDIService __far *)MK_FP(r.w.si, r.w.cx);
    if (memcmp(ms->msname, "MIDS", 4) != 0) {
        printf("Services structure is not tagged MIDS.\n");
        ms = 0;
        return 1;
    }

    (ms->msGlobalReset)();

    base_ticks = *(unsigned long __far *)MK_FP(0x40, 0x6C);
    cur_tick  = 0;
    cur_us    = 0;
    midi_start = now_us();
    midi_due_valid = 0;
    return 0;
}

/* One non-blocking pass of the sequencer.  Returns 0 once every track has
 * ended.  It never waits: if the next event is not due yet it returns and
 * lets the caller get on with the WAVE device, which is what makes running
 * both at once possible. */
static int midi_poll(void)
{
    int i, t;

    if (ms == 0) return 0;

    if (!midi_due_valid) {
        unsigned long soonest = 0xFFFFFFFFUL;
        int any = 0;

        for (i = 0; i < ntracks; i++) {
            if (tracks[i].done) continue;
            any = 1;
            if (tracks[i].nexttick < soonest) soonest = tracks[i].nexttick;
        }
        if (!any) return 0;

        /* advance_to() accumulates, so it must be called exactly once per
         * tick -- hence the latch.  It is called before the events at this
         * tick are emitted, so a tempo change here governs the interval that
         * follows it, not the one before. */
        midi_due_tick  = soonest;
        midi_due_us    = midi_start + advance_to(soonest);
        midi_due_valid = 1;
    }

    if (now_us() < midi_due_us) return 1;        /* not yet -- come back */

    for (t = 0; t < ntracks; t++) {
        while (!tracks[t].done && tracks[t].nexttick == midi_due_tick) {
            if (!play_event(ms, &tracks[t])) break;
            if (tracks[t].pos >= tracks[t].end) { tracks[t].done = 1; break; }
            tracks[t].nexttick = midi_due_tick + read_vlq(&tracks[t].pos);
        }
    }

    midi_due_valid = 0;
    return 1;
}

static void midi_teardown(void)
{
    union REGS r;

    if (ms != 0) {
        (ms->msGlobalReset)();
        ms = 0;
    }
    if (hMIDI != 0) {
        r.w.ax = VESAFUNCID;
        r.w.bx = VF_CLOSE;
        r.w.cx = (unsigned)hMIDI;
        int86(0x10, &r, &r);
        hMIDI = 0;
    }
    if (midi_memseg) { _dos_freemem(midi_memseg); midi_memseg = 0; }
    if (mid_seg)     { _dos_freemem(mid_seg);     mid_seg = 0; }
}


/* ------------------------------------------------------------------ */
/* WAVE playback                                                      */
/* ------------------------------------------------------------------ */

/* Everything up to, but not including, starting the block.  Kept separate
 * from wave_start() so that when both devices are used the WAVE block is
 * only set going once the MIDI file has been loaded and its device opened,
 * which is what puts the two in step at the top of the loop. */
static int wave_setup(const char *path)
{
    union REGS r;
    GeneralDeviceClass gdc;
    WavInfo wav;
    FILE *fp;
    unsigned long done;
    int fh;
    long best;

    /* --- subfunction 1: find a WAVE device --- */

    r.w.ax = VESAFUNCID;
    r.w.bx = VF_LOCATE;
    r.w.cx = 0;
    r.w.dx = WAVDEVICE;
    int86(0x10, &r, &r);
    if (r.w.ax != VESAOK || r.w.cx == 0) {
        printf("No VBE/AI WAVE device found.\n");
        return 1;
    }
    hWave = (int)r.w.cx;

    /* --- subfunction 2, query 2: what kind of device is it? --- */

    r.w.ax = VESAFUNCID;
    r.w.bx = VF_QUERY;
    r.w.cx = (unsigned)hWave;
    r.w.dx = Q_GDC_COPY;
    r.w.si = FP_SEG((void __far *)&gdc);
    r.w.di = FP_OFF((void __far *)&gdc);
    int86(0x10, &r, &r);
    if (r.w.ax != VESAOK) {
        printf("Device query failed (AX=%04X).\n", r.w.ax);
        return 1;
    }

    printf("WAVE:   %s / %s (%s)\n", gdc.wi.wivname, gdc.wi.wiprod, gdc.wi.wichip);
    printf("        features=%08lX memreq=%u ticks/sec=%u\n",
           gdc.wi.wifeatures, (unsigned)gdc.wi.wimemreq,
           (unsigned)gdc.wi.witimerticks);

    /* --- read the WAV file --- */

    fp = fopen(path, "rb");
    if (fp == NULL) { printf("Cannot open %s\n", path); return 1; }
    if (!read_wav_header(fp, &wav)) {
        printf("Cannot parse %s\n", path);
        fclose(fp); return 1;
    }
    printf("%s: %d ch, %ld Hz, %d bit, %lu bytes\n",
           path, wav.channels, wav.rate, wav.bits, wav.data_len);

    if (wav.data_len == 0) {
        printf("No sample data.\n"); fclose(fp); return 1;
    }

    /* --- allocate the driver's block and the sample buffer --- */

    if (_dos_allocmem((unsigned)((gdc.wi.wimemreq + 15) / 16), &memblock_seg) != 0) {
        printf("Out of memory for the driver block.\n");
        fclose(fp); return 1;
    }
    if (_dos_allocmem((unsigned)((wav.data_len + 15UL) / 16UL), &data_seg) != 0) {
        printf("Out of memory for %lu bytes of samples.\n", wav.data_len);
        fclose(fp); return 1;
    }

    /* Read the samples through a raw DOS handle rather than the stdio stream
     * used for parsing: the two keep separate file positions, and a far
     * destination needs _dos_read anyway. */
    fclose(fp);

    if (_dos_open(path, O_RDONLY, &fh) != 0) {
        printf("Cannot reopen %s\n", path);
        return 1;
    }
    if (lseek(fh, wav.data_off, SEEK_SET) == -1L) {
        printf("Seek to sample data failed.\n");
        _dos_close(fh); return 1;
    }

    done = 0;
    while (done < wav.data_len) {
        unsigned long left = wav.data_len - done;
        unsigned chunk = (left > 0x8000UL) ? 0x8000u : (unsigned)left;
        unsigned got = 0;
        void __far *dst = MK_FP((unsigned)(data_seg + (unsigned)(done >> 4)),
                                (unsigned)(done & 0x0FUL));
        if (_dos_read(fh, dst, chunk, &got) != 0) {
            printf("Read failed at offset %lu.\n", done);
            _dos_close(fh); return 1;
        }
        if (got == 0) {                 /* short file: play what we have */
            printf("Short file: %lu of %lu bytes.\n", done, wav.data_len);
            wav.data_len = done;
            break;
        }
        done += got;
    }
    _dos_close(fh);

    if (wav.data_len == 0) {
        printf("No sample data read.\n");
        return 1;
    }

    /* --- subfunction 3: open the device --- */

    r.w.ax = VESAFUNCID;
    r.w.bx = VF_OPEN;
    r.w.cx = (unsigned)hWave;
    r.w.dx = 0;                          /* 16-bit interface */
    r.w.si = memblock_seg;               /* offset is assumed zero */
    int86(0x10, &r, &r);
    if (r.w.ax != VESAOK || (r.w.si == 0 && r.w.cx == 0)) {
        printf("Open failed (AX=%04X).\n", r.w.ax);
        return 1;
    }
    ws = (WAVEService __far *)MK_FP(r.w.si, r.w.cx);

    if (memcmp(ws->wsname, "WAVS", 4) != 0) {
        printf("Services structure is not tagged WAVS.\n");
        ws = 0;
        return 1;
    }

    ws->wsApplPSyncCB = PlayDone;
    ws->wsApplRSyncCB = 0;

    /* --- set the stream format --- */

    best = ws->wsPCMInfo(wav.channels, wav.rate, 0, 0, wav.bits);
    if (best == 0) {
        printf("wsPCMInfo rejected the format (error %d).\n", ws->wsGetLastError());
        return 1;
    }
    if (best != wav.rate) printf("Driver chose %ld Hz.\n", best);

    /* --- register the sample block --- */

    block_handle = ws->wsWaveRegister((void __far *)MK_FP(data_seg, 0),
                                      (long)wav.data_len);
    if (block_handle == 0) {
        printf("wsWaveRegister failed (error %d).\n", ws->wsGetLastError());
        return 1;
    }

    return 0;
}

static int wave_start(void)
{
    if (!ws->wsPlayBlock(block_handle, 0L)) {
        printf("wsPlayBlock failed (error %d).\n", ws->wsGetLastError());
        return 1;
    }
    return 0;
}

/* One pass of the driver's timer tick.  Returns 0 when the block has
 * finished.  Calling more often than witimerticks is harmless; this is the
 * application's half of the bargain, and also where the completion callback
 * gets delivered. */
static int wave_poll(void)
{
    if (ws == 0) return 0;

    ws->wsTimerTick();

    if (playback_done) return 0;

    /* backstop in case the callback never arrives */
    if ((int)ws->wsDeviceCheck(WAVEDRIVERSTATE, 0L) == 0) return 0;

    return 1;
}

/* ------------------------------------------------------------------ */

#define KIND_NONE   0
#define KIND_WAVE   1
#define KIND_MIDI   2

/* Sniff the file rather than trusting the extension. */
static int file_kind(const char *path)
{
    FILE *f = fopen(path, "rb");
    char hdr[12];
    size_t n;

    if (f == NULL) return KIND_NONE;
    n = fread(hdr, 1, 12, f);
    fclose(f);

    if (n >= 4 && memcmp(hdr, "MThd", 4) == 0) return KIND_MIDI;
    if (n >= 12 && memcmp(hdr, "RIFF", 4) == 0 &&
                   memcmp(hdr + 8, "WAVE", 4) == 0) return KIND_WAVE;
    return KIND_NONE;
}

static void usage(void)
{
    printf("Usage: VBEAIWAV [file.wav] [file.mid]\n");
    printf("       Give both to play them together; either alone plays alone.\n");
    printf("       With no arguments, plays TEST.WAV.\n");
}

int main(int argc, char *argv[])
{
    const char *wavpath = 0;
    const char *midpath = 0;
    union REGS r;
    int i, wave_on = 0, midi_on = 0, stopped = 0;

    /* --- subfunction 0: is a VBE/AI provider there at all? --- */

    r.w.ax = VESAFUNCID;
    r.w.bx = VF_DRIVERCHECK;
    r.w.cx = 0;
    int86(0x10, &r, &r);
    if (r.w.ax != VESAOK) {
        printf("No VBE/AI interface present (AX=%04X).\n", r.w.ax);
        printf("Is 'vbeai=true' set in the [vbeai] section of dosbox-x.conf?\n");
        return 1;
    }
    printf("VBE/AI version %d.%d present.\n",
           (r.h.bl >> 4) & 0x0f, r.h.bl & 0x0f);

    /* --- work out what was asked for --- */

    if (argc < 2) {
        wavpath = "TEST.WAV";
    }
    else for (i = 1; i < argc; i++) {
        switch (file_kind(argv[i])) {
        case KIND_WAVE:
            if (wavpath) { printf("Only one WAVE file, please.\n"); usage(); return 1; }
            wavpath = argv[i];
            break;
        case KIND_MIDI:
            if (midpath) { printf("Only one MIDI file, please.\n"); usage(); return 1; }
            midpath = argv[i];
            break;
        default:
            printf("%s: not a RIFF/WAVE or Standard MIDI File.\n", argv[i]);
            usage();
            return 1;
        }
    }

    /* --- open both devices before either is set going --- */

    if (wavpath && wave_setup(wavpath) != 0) { cleanup(); return 1; }
    if (midpath && midi_setup(midpath) != 0) { midi_teardown(); cleanup(); return 1; }

    if (wavpath) {
        if (wave_start() != 0) { midi_teardown(); cleanup(); return 1; }
        wave_on = 1;
    }
    midi_on = (midpath != 0);

    printf("Playing %s -- ESC to stop.\n",
           (wave_on && midi_on) ? "both" : (wave_on ? "WAVE" : "MIDI"));

    /* Two devices, one loop, neither blocking.  wave_poll() services the
     * driver's timer tick and midi_poll() releases whatever the sequencer
     * has fallen due; both return promptly, so the one that is still going
     * keeps being serviced after the other has finished. */
    while (wave_on || midi_on) {
        if (wave_on) wave_on = wave_poll();
        if (midi_on) midi_on = midi_poll();

        if (kbhit() && getch() == 0x1b) {
            stopped = 1;
            if (wave_on) ws->wsStopIO(0);
            break;
        }
    }

    printf(stopped ? "Stopped.\n" : "Playback complete.\n");

    midi_teardown();
    cleanup();
    return 0;
}
