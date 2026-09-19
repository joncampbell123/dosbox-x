/*
 *  vbeaiwav -- minimal VESA VBE/AI WAVE playback test for DOSBox-X
 *
 *  Plays a RIFF/WAVE file through INT 10h AX=4F13h.  Its only purpose is to
 *  prove the DOSBox-X built-in VBE/AI provider end to end; it is deliberately
 *  not a general-purpose player.
 *
 *  Build with Open Watcom (16-bit real mode, large model):
 *
 *      wcl -0 -ml -bcl=dos -fe=vbeaiwav.exe vbeaiwav.c
 *
 *  Usage:  VBEAIWAV [file.wav]        (defaults to TEST.WAV)
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

#pragma pack(pop)

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

int main(int argc, char *argv[])
{
    const char *path = (argc > 1) ? argv[1] : "TEST.WAV";
    union REGS r;
    GeneralDeviceClass gdc;
    WavInfo wav;
    FILE *fp;
    unsigned long done;
    int fh;
    long best;
    int state;

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
        hWave = 0;
        return 1;
    }

    printf("Device: %s / %s (%s)\n", gdc.wi.wivname, gdc.wi.wiprod, gdc.wi.wichip);
    printf("        features=%08lX memreq=%u ticks/sec=%u\n",
           gdc.wi.wifeatures, (unsigned)gdc.wi.wimemreq,
           (unsigned)gdc.wi.witimerticks);

    /* --- read the WAV file --- */

    fp = fopen(path, "rb");
    if (fp == NULL) { printf("Cannot open %s\n", path); hWave = 0; return 1; }
    if (!read_wav_header(fp, &wav)) {
        printf("Cannot parse %s\n", path);
        fclose(fp); hWave = 0; return 1;
    }
    printf("%s: %d ch, %ld Hz, %d bit, %lu bytes\n",
           path, wav.channels, wav.rate, wav.bits, wav.data_len);

    if (wav.data_len == 0) {
        printf("No sample data.\n"); fclose(fp); hWave = 0; return 1;
    }

    /* --- allocate the driver's block and the sample buffer --- */

    if (_dos_allocmem((unsigned)((gdc.wi.wimemreq + 15) / 16), &memblock_seg) != 0) {
        printf("Out of memory for the driver block.\n");
        fclose(fp); hWave = 0; return 1;
    }
    if (_dos_allocmem((unsigned)((wav.data_len + 15UL) / 16UL), &data_seg) != 0) {
        printf("Out of memory for %lu bytes of samples.\n", wav.data_len);
        fclose(fp); cleanup(); return 1;
    }

    /* Read the samples through a raw DOS handle rather than the stdio stream
     * used for parsing: the two keep separate file positions, and a far
     * destination needs _dos_read anyway. */
    fclose(fp);
    fp = NULL;

    if (_dos_open(path, O_RDONLY, &fh) != 0) {
        printf("Cannot reopen %s\n", path);
        cleanup(); return 1;
    }
    if (lseek(fh, wav.data_off, SEEK_SET) == -1L) {
        printf("Seek to sample data failed.\n");
        _dos_close(fh); cleanup(); return 1;
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
            _dos_close(fh); cleanup(); return 1;
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
        cleanup(); return 1;
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
        cleanup(); return 1;
    }
    ws = (WAVEService __far *)MK_FP(r.w.si, r.w.cx);

    if (memcmp(ws->wsname, "WAVS", 4) != 0) {
        printf("Services structure is not tagged WAVS.\n");
        cleanup(); return 1;
    }

    ws->wsApplPSyncCB = PlayDone;
    ws->wsApplRSyncCB = 0;

    /* --- set the stream format --- */

    best = ws->wsPCMInfo(wav.channels, wav.rate, 0, 0, wav.bits);
    if (best == 0) {
        printf("wsPCMInfo rejected the format (error %d).\n", ws->wsGetLastError());
        cleanup(); return 1;
    }
    if (best != wav.rate) printf("Driver chose %ld Hz.\n", best);

    /* --- register the sample block and start it --- */

    block_handle = ws->wsWaveRegister((void __far *)MK_FP(data_seg, 0),
                                      (long)wav.data_len);
    if (block_handle == 0) {
        printf("wsWaveRegister failed (error %d).\n", ws->wsGetLastError());
        cleanup(); return 1;
    }

    if (!ws->wsPlayBlock(block_handle, 0L)) {
        printf("wsPlayBlock failed (error %d).\n", ws->wsGetLastError());
        cleanup(); return 1;
    }

    printf("Playing -- ESC to stop.\n");

    /* The driver asked for timer ticks, so drive it.  Calling more often than
     * witimerticks is harmless; this loop is the application's half of the
     * bargain and is also where the completion callback gets delivered. */
    for (;;) {
        ws->wsTimerTick();

        if (playback_done) break;

        /* backstop in case the callback never arrives */
        state = (int)ws->wsDeviceCheck(WAVEDRIVERSTATE, 0L);
        if (state == 0) break;                   /* idle again */

        if (kbhit() && getch() == 0x1b) {
            ws->wsStopIO(0);
            printf("Stopped.\n");
            break;
        }
    }

    if (playback_done) printf("Playback complete.\n");

    cleanup();
    return 0;
}
