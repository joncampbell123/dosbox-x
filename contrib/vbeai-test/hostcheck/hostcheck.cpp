/* White-box harness for src/ints/int10_vesa_ai.cpp.
 *
 * Runs the provider against a fake guest memory and register file so the
 * Pascal frame arithmetic, structure layout and playback stepping can be
 * checked without the emulator. The provider source is #included so its
 * statics are visible. */

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <utility>

#include "dosbox.h"
#include "regs.h"
#include "cpu.h"
#include "mem.h"
#include "mixer.h"
#include "callback.h"
#include "setup.h"
#include "control.h"
#include "logging.h"
#include "hardware/dbopl.h"

/* ---- stub state ---- */
uint8_t GuestMem[0x110000];
RegFile R;
CPUBlock cpu;
bool log_verbose = false;
Config* control;
MIXER_Handler TheMixerHandler = NULL;

static MixerChannel TheChannel;
static MixerChannel TheFMChannel;
static Section_prop TheSection;
static bool section_enable = true;

static const char *section_midimode = "auto";
bool Section_prop::Get_bool(const char*) const { return section_enable; }
const char *Section_prop::Get_string(const char*) const { return section_midimode; }
Section* Config::GetSection(const char*) const { return (Section*)&TheSection; }

static Bitu rom_next = 0xF8000;
Bitu ROMBIOS_GetMemory(Bitu bytes,const char*,Bitu,Bitu){ Bitu r=rom_next; rom_next+=bytes; return r; }

uint8_t CALLBACK_Allocate(){ return 42; }
bool CALLBACK_Setup(Bitu,CallBack_Handler,Bitu,const char*){ return true; }

MixerChannel* MIXER_AddChannel(MIXER_Handler h,Bitu f,const char* name){
    if (name && strcmp(name,"VBEAIFM")==0) { TheFMChannel.freq=f; return &TheFMChannel; }
    TheMixerHandler=h; TheChannel.freq=f; return &TheChannel;
}

/* every byte the provider emits, in order */
static std::vector<uint8_t> MidiOut;
static bool midi_present = true;
void MIDI_RawOutByte(uint8_t data) { MidiOut.push_back(data); }
bool MIDI_Available(void) { return midi_present; }

/* every OPL register write the synthesiser makes */
std::vector<std::pair<uint32_t,uint8_t> > OplWrites;
uint8_t OplRegs[512];
void DBOPL::Handler::WriteReg(uint32_t addr, uint8_t val) {
    OplWrites.push_back(std::make_pair(addr, val));
    if (addr < 512) OplRegs[addr] = val;
}


/* ---- the code under test ---- */
#include "vbeai_fm.cpp"
#include "int10_vesa_ai.cpp"

/* ---- test scaffolding ---- */

static int failures = 0;

static void check(const char *what, long long got, long long want) {
    if (got == want) printf("  ok    %-52s = %lld\n", what, got);
    else { printf("  FAIL  %-52s = %lld (want %lld)\n", what, got, want); failures++; }
}

#define STACK_SEG 0x3000
#define STACK_SP  0x1000
#define RET_IP    0xBEEF
#define RET_CS    0x1234

/* Build the stack image for a Pascal call. Arguments are given in DECLARED
 * order as (byte size, value); they are laid out in reverse, so the last
 * declared argument lands at SP+4. */
static void CallService(int index, const std::vector<std::pair<int,uint32_t> > &decl) {
    R.seg[ss] = STACK_SEG;
    reg_esp = STACK_SP;
    PhysPt sp = SegPhys(ss) + reg_esp;
    phys_writew(sp + 0, RET_IP);
    phys_writew(sp + 2, RET_CS);

    PhysPt at = sp + 4;
    for (size_t i = decl.size(); i-- > 0; ) {
        if (decl[i].first == 2) { phys_writew(at, (uint16_t)decl[i].second); at += 2; }
        else                    { phys_writed(at, decl[i].second);           at += 4; }
    }
    reg_ax = (uint16_t)index;
    VBEAI_ServiceHandler();
}

static uint32_t RetLong(void) { return ((uint32_t)reg_dx << 16) | reg_ax; }

int main(void) {
    cpu.stack.mask = 0xFFFF;
    cpu.stack.notmask = 0xFFFF0000;
    memset(GuestMem, 0, sizeof(GuestMem));

    printf("== setup ==\n");
    VBEAI_Setup();
    check("enabled", vbeai.enabled, 1);
    check("mixer channel created", vbeai.chan != NULL, 1);

    /* --- ROM trampolines --- */
    printf("== ROM trampolines ==\n");
    {
        PhysPt s = PhysMake(RealSeg(vbeai_stubs), RealOff(vbeai_stubs));
        bool shape_ok = true, retf_ok = true;
        for (int i = 0; i < VF_COUNT; i++) {
            PhysPt a = s + i * VBEAI_STUB_SIZE;
            if (phys_readb(a) != 0xB8 || phys_readw(a+1) != i) shape_ok = false;
            if (phys_readb(a+3) != 0xFE || phys_readb(a+4) != 0x38) shape_ok = false;
            if (phys_readb(a+7) != 0xCA) shape_ok = false;
            if (phys_readw(a+8) != vbeai_func_argbytes[i]) retf_ok = false;
        }
        check("all 14 stubs are MOV AX,i / callback / RETF", shape_ok, 1);
        check("every RETF operand matches its arg bytes", retf_ok, 1);
        check("wsPCMInfo stub RETF operand", phys_readw(s + VF_PCMINFO*VBEAI_STUB_SIZE + 8), 12);
        check("wsWaveRegister stub RETF operand", phys_readw(s + VF_WAVEREGISTER*VBEAI_STUB_SIZE + 8), 8);
    }

    /* --- subfunction 0 / 1 --- */
    printf("== INT 10h discovery ==\n");
    reg_bx = 0x0000; reg_cx = 0;
    check("fn0 handled", INT10_VBEAI_Handler(), 1);
    check("fn0 AX", reg_ax & 0xFFFF, 0x004F);
    check("fn0 BL version", reg_bl, 0x10);

    reg_bx = 0x0001; reg_cx = 0; reg_dx = 0x0001;
    INT10_VBEAI_Handler();
    check("fn1 first call returns handle", reg_cx, 1);
    reg_bx = 0x0001; reg_cx = 1; reg_dx = 0x0001;
    INT10_VBEAI_Handler();
    check("fn1 second call returns 0", reg_cx, 0);
    reg_bx = 0x0001; reg_cx = 0; reg_dx = 0x0003;   /* Volume: not provided */
    INT10_VBEAI_Handler();
    check("fn1 unprovided class returns 0", reg_cx, 0);

    /* --- subfunction 2: GeneralDeviceClass --- */
    printf("== query device class ==\n");
    reg_bx = 0x0002; reg_cx = 1; reg_dx = 0x0001;
    INT10_VBEAI_Handler();
    check("fn2 q1 length in SI:DI", ((uint32_t)reg_si<<16)|reg_di, 138);

    reg_bx = 0x0002; reg_cx = 1; reg_dx = 0x0002;
    reg_si = 0x1000; reg_di = 0x0000;
    INT10_VBEAI_Handler();
    check("fn2 q2 leaves SI:DI as the buffer", ((uint32_t)reg_si<<16)|reg_di, 0x10000000);
    {
        PhysPt g = PhysMake(0x1000,0);
        check("gdname 'VESA'", phys_readd(g)==0x41534556, 1);
        check("gdlength", phys_readd(g+4), 138);
        check("gdclassid WAVE", phys_readw(g+8), 1);
        PhysPt w = g + 12;
        check("winame 'WAVI'", phys_readd(w)==0x49564157, 1);
        check("wilength", phys_readd(w+4), 126);
        check("wifeatures", phys_readd(w+112), 0x3004A529UL);
        check("wimemreq", phys_readw(w+118), 128);
        check("witimerticks", phys_readw(w+120), 100);
        check("wiChannels", phys_readw(w+122), 2);
        check("wiSampleSize", phys_readw(w+124), 3);
    }

    /* --- subfunction 3: open --- */
    printf("== open device ==\n");
    reg_bx = 0x0003; reg_cx = 1; reg_dx = 0; reg_si = 0x2000;
    INT10_VBEAI_Handler();
    check("fn3 AX", reg_ax & 0xFFFF, 0x004F);
    check("fn3 SI = block segment", reg_si, 0x2000);
    check("fn3 CX = offset", reg_cx, 0);
    {
        PhysPt s = PhysMake(0x2000,0);
        check("wsname 'WAVS'", phys_readd(s)==0x53564157, 1);
        check("wslength", phys_readd(s+4), 84);
        bool ptrs_ok = true;
        for (int i = 0; i < VF_WAVE_COUNT; i++)
            if (phys_readd(s+24+i*4) != (uint32_t)VBEAI_StubPtr((uint16_t)i)) ptrs_ok = false;
        check("13 service pointers point at the ROM stubs", ptrs_ok, 1);
        check("wsApplPSyncCB starts NULL", phys_readd(s+76), 0);
    }

    /* --- wsPCMInfo --- */
    printf("== wsPCMInfo ==\n");
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2,1));      /* channels */
      a.push_back(std::make_pair(4,22050));  /* rate     */
      a.push_back(std::make_pair(2,0));      /* comp     */
      a.push_back(std::make_pair(2,0));      /* blocking */
      a.push_back(std::make_pair(2,8));      /* pcmsize  */
      CallService(VF_PCMINFO, a); }
    check("returns best-match rate in DX:AX", RetLong(), 22050);
    check("channels decoded", vbeai.channels, 1);
    check("bits decoded", vbeai.bits, 8);
    check("rate decoded", vbeai.rate, 22050);
    check("mixer channel retuned", vbeai.chan->freq, 22050);

    /* a rejected format must not disturb the accepted one */
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2,3)); a.push_back(std::make_pair(4,22050));
      a.push_back(std::make_pair(2,0)); a.push_back(std::make_pair(2,0));
      a.push_back(std::make_pair(2,8));
      CallService(VF_PCMINFO, a); }
    check("3 channels rejected", RetLong(), 0);
    check("channels unchanged after rejection", vbeai.channels, 1);

    /* --- wsWaveRegister + wsPlayBlock --- */
    printf("== register and play a block ==\n");
    const int NSAMP = 1000;
    for (int i = 0; i < NSAMP; i++) phys_writeb(PhysMake(0x4000,0)+i, (uint8_t)(i & 0xFF));

    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(4, ((uint32_t)0x4000<<16)|0));  /* void huge * */
      a.push_back(std::make_pair(4, NSAMP));                     /* length      */
      CallService(VF_WAVEREGISTER, a); }
    int bh = reg_ax;
    check("wsWaveRegister returns handle 1", bh, 1);
    check("block length recorded", vbeai.blocks[1].len, NSAMP);
    check("block address recorded", vbeai.blocks[1].addr, PhysMake(0x4000,0));

    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2, bh)); a.push_back(std::make_pair(4, 0));
      CallService(VF_PLAYBLOCK, a); }
    check("wsPlayBlock returns TRUE", reg_ax, 1);
    check("device is busy", vbeai.playing, 1);
    check("mixer channel enabled", vbeai.chan->on, 1);

    /* device check while playing, through the service call */
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2, WAVEDRIVERSTATE)); a.push_back(std::make_pair(4, 0));
      CallService(VF_DEVICECHECK, a); }
    check("WAVEDRIVERSTATE says busy", RetLong(), 1);

    /* --- drain through the mixer --- */
    printf("== mixer drain ==\n");
    for (int i = 0; i < 100 && vbeai.playing; i++) TheMixerHandler(64);
    check("playback finished", vbeai.playing, 0);
    check("mixer received every sample", (long long)vbeai.chan->got.size(), NSAMP);
    {
        bool data_ok = true;
        for (int i = 0; i < NSAMP; i++)
            if (vbeai.chan->got[i] != (int16_t)(((int)(uint8_t)(i & 0xFF) - 128) << 8)) data_ok = false;
        check("samples match the source bytes", data_ok, 1);
    }
    check("one callback queued", vbeai.pending.count, 1);

    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2, WAVEDRIVERSTATE)); a.push_back(std::make_pair(4, 0));
      CallService(VF_DEVICECHECK, a); }
    check("WAVEDRIVERSTATE says idle", RetLong(), 0);

    /* --- completion callback delivery --- */
    printf("== wsApplPSyncCB frame ==\n");
    phys_writed(PhysMake(0x2000,0) + 76, ((uint32_t)0x5555 << 16) | 0x1111);
    {
        std::vector<std::pair<int,uint32_t> > none;
        CallService(VF_TIMERTICK, none);
    }
    check("11 words pushed", (long)(STACK_SP - reg_esp), 22);
    {
        PhysPt sp = SegPhys(ss) + reg_esp;
        check("RETF target offset = app callback", phys_readw(sp+0), 0x1111);
        check("RETF target segment = app callback", phys_readw(sp+2), 0x5555);
        /* what the application sees once its far entry is taken */
        PhysPt f = sp + 4;
        check("callback retIP  = syncret stub", phys_readw(f+0), RealOff(VBEAI_StubPtr(VF_SYNCRET)));
        check("callback retCS  = syncret stub", phys_readw(f+2), RealSeg(VBEAI_StubPtr(VF_SYNCRET)));
        check("arg4 reserved (long) = 0", phys_readd(f+4), 0);
        check("arg3 length (long)", phys_readd(f+8), NSAMP);
        check("arg2 ptr offset", phys_readw(f+12), 0);
        check("arg2 ptr segment", phys_readw(f+14), 0x4000);
        check("arg1 device handle", phys_readw(f+16), 1);
        /* after the application's RETF 14 the original return address is on top */
        check("RETF 14 lands back on the caller's IP", phys_readw(f+18), RET_IP);
        check("RETF 14 lands back on the caller's CS", phys_readw(f+20), RET_CS);
    }
    check("queue drained", vbeai.pending.count, 0);

    /* --- wsPlayCont: circular buffer with division callbacks --- */
    printf("== wsPlayCont ==\n");
    vbeai.chan->got.clear();
    for (int i = 0; i < 512; i++) phys_writeb(PhysMake(0x6000,0)+i, (uint8_t)(0x80 + (i & 0x0F)));

    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(4, ((uint32_t)0x6000<<16)|0));  /* void far * */
      a.push_back(std::make_pair(4, 512));                       /* length     */
      a.push_back(std::make_pair(4, 128));                       /* division   */
      CallService(VF_PLAYCONT, a); }
    check("wsPlayCont returns TRUE", reg_ax, 1);
    check("continuous mode", vbeai.continuous, 1);
    check("division length", vbeai.divlen, 128);

    /* a buffer crossing the 64K page must be refused */
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(4, ((uint32_t)0x7000<<16)|0xFF00));
      a.push_back(std::make_pair(4, 512));
      a.push_back(std::make_pair(4, 128));
      CallService(VF_PLAYCONT, a); }
    check("buffer crossing 64K refused", reg_ax, 0);
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2, 0)); a.push_back(std::make_pair(4, 0));
      CallService(VF_GETLASTERROR, a); }
    check("  reported as WAV_BADBLOCKADDR", reg_ax, 4);

    /* restart it -- the refused call did not disturb the running stream */
    check("still playing after the refusal", vbeai.playing, 1);
    vbeai.chan->got.clear();
    vbeai.pending.clear();

    /* drain exactly one lap of the ring */
    for (int i = 0; i < 8; i++) TheMixerHandler(64);
    check("still running (circular)", vbeai.playing, 1);
    check("one lap rendered", (long long)vbeai.chan->got.size(), 512);
    check("four division callbacks queued", vbeai.pending.count, 4);
    {
        bool ptrs_ok = true;
        for (unsigned d = 0; d < 4; d++) {
            const VBEAI_Pending &q = vbeai.pending.item[(vbeai.pending.head + d) % VBEAI_PENDING_MAX];
            if (RealSeg(q.ptr) != 0x6000 || RealOff(q.ptr) != d * 128 || q.len != 128) ptrs_ok = false;
        }
        check("each points at its own division, len 128", ptrs_ok, 1);
    }
    check("ring wrapped back to the start", vbeai.playpos, 0);

    { std::vector<std::pair<int,uint32_t> > a; a.push_back(std::make_pair(2, 0));
      CallService(VF_PAUSEIO, a); }
    check("wsPauseIO returns TRUE", reg_ax, 1);
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2, WAVEDRIVERSTATE)); a.push_back(std::make_pair(4, 0));
      CallService(VF_DEVICECHECK, a); }
    check("WAVEDRIVERSTATE busy|paused", RetLong(), 0x81);
    { std::vector<std::pair<int,uint32_t> > a; a.push_back(std::make_pair(2, 0));
      CallService(VF_RESUMEIO, a); }
    check("wsResumeIO returns TRUE", reg_ax, 1);
    { std::vector<std::pair<int,uint32_t> > a; a.push_back(std::make_pair(2, 0));
      CallService(VF_STOPIO, a); }
    check("wsStopIO stops the stream", vbeai.playing, 0);

    /* --- record is not provided --- */
    printf("== unsupported paths ==\n");
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2, 1)); a.push_back(std::make_pair(4, 0));
      CallService(VF_RECORDBLOCK, a); }
    check("wsRecordBlock returns FALSE", reg_ax, 0);
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2, 0)); a.push_back(std::make_pair(4, 0));
      CallService(VF_GETLASTERROR, a); }
    check("  reported as WAV_NOSUPPORT", reg_ax, 1);
    check("  and the error is cleared once read", vbeai.lasterror, 0);

    /* --- close --- */
    printf("== close ==\n");
    reg_bx = 0x0004; reg_cx = 1;
    INT10_VBEAI_Handler();
    check("fn4 AX", reg_ax & 0xFFFF, 0x004F);
    check("closed", vbeai.opened, 0);
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2, WAVEDRIVERSTATE)); a.push_back(std::make_pair(4, 0));
      CallService(VF_DEVICECHECK, a); }
    check("services inert after close", RetLong(), 0);

    /* ================= MIDI device ================= */

    printf("== MIDI enumeration ==\n");
    reg_bx = 0x0001; reg_cx = 0; reg_dx = 0x0000;   /* any class */
    INT10_VBEAI_Handler();
    check("any-class first  -> WAVE handle", reg_cx, 1);
    reg_bx = 0x0001; reg_cx = 1; reg_dx = 0x0000;
    INT10_VBEAI_Handler();
    check("any-class second -> MIDI handle", reg_cx, 2);
    reg_bx = 0x0001; reg_cx = 2; reg_dx = 0x0000;
    INT10_VBEAI_Handler();
    check("any-class third  -> none left", reg_cx, 0);

    reg_bx = 0x0001; reg_cx = 0; reg_dx = 0x0002;   /* MIDI class */
    INT10_VBEAI_Handler();
    check("MIDI class -> handle 2", reg_cx, 2);
    reg_bx = 0x0001; reg_cx = 2; reg_dx = 0x0002;
    INT10_VBEAI_Handler();
    check("MIDI class -> none left", reg_cx, 0);

    /* with no MIDI output configured the device must not be offered at all */
    midi_present = false;
    reg_bx = 0x0001; reg_cx = 0; reg_dx = 0x0002;
    INT10_VBEAI_Handler();
    check("no MIDI output -> device not enumerated", reg_cx, 0);
    reg_bx = 0x0001; reg_cx = 1; reg_dx = 0x0000;
    INT10_VBEAI_Handler();
    check("no MIDI output -> any-class stops after WAVE", reg_cx, 0);
    midi_present = true;

    printf("== MIDI device class ==\n");
    reg_bx = 0x0002; reg_cx = 2; reg_dx = 0x0001;
    INT10_VBEAI_Handler();
    check("q1 MIDI GDC length", ((uint32_t)reg_si<<16)|reg_di, 150);

    reg_bx = 0x0002; reg_cx = 2; reg_dx = 0x0002;
    reg_si = 0x3000; reg_di = 0x0000;
    INT10_VBEAI_Handler();
    {
        PhysPt g = PhysMake(0x3000,0);
        check("gdname 'VESA'", phys_readd(g)==0x41534556, 1);
        check("gdlength", phys_readd(g+4), 150);
        check("gdclassid MIDI", phys_readw(g+8), 2);
        PhysPt m = g + 12;
        check("miname 'MIDI'", phys_readd(m)==0x4944494D, 1);
        check("milength", phys_readd(m+4), 138);
        check("milibrary empty (patches preloaded)", phys_readb(m+112), 0);
        check("mifeatures = XMITR|PRELD", phys_readd(m+126), 0x30);
        check("mimemreq", phys_readw(m+132), 128);
        check("mitimerticks (none needed)", phys_readw(m+134), 0);
        check("miactivetones (unknowable)", phys_readw(m+136), 0xFFFF);
    }

    printf("== MIDI open ==\n");
    reg_bx = 0x0003; reg_cx = 2; reg_dx = 0; reg_si = 0x4000;
    INT10_VBEAI_Handler();
    check("fn3 AX", reg_ax & 0xFFFF, 0x004F);
    check("fn3 SI = block segment", reg_si, 0x4000);
    {
        PhysPt s = PhysMake(0x4000,0);
        check("msname 'MIDS'", phys_readd(s)==0x5344494D, 1);
        check("mslength", phys_readd(s+4), 96);
        bool patches_ok = true;
        for (int i = 0; i < 16; i++) if (phys_readw(s+8+i*2) != 0xFFFF) patches_ok = false;
        check("mspatches: all present", patches_ok, 1);
        bool ptrs_ok = true;
        for (int i = 0; i < VF_MIDI_COUNT; i++)
            if (phys_readd(s+56+i*4) != (uint32_t)VBEAI_StubPtr((uint16_t)(VF_MS_DEVICECHECK+i))) ptrs_ok = false;
        check("8 service pointers point at the ROM stubs", ptrs_ok, 1);
        check("msApplFreeCB starts NULL", phys_readd(s+88), 0);
        check("msApplMIDIIn starts NULL", phys_readd(s+92), 0);
    }

    printf("== msMIDImsg ==\n");
    {
        /* Note On ch0 middle C, Note Off, with a running-status second note */
        const uint8_t msg[] = { 0x90, 0x3C, 0x7F, 0x3E, 0x60, 0x80, 0x3C, 0x00 };
        for (unsigned i = 0; i < sizeof(msg); i++) phys_writeb(PhysMake(0x5000,0)+i, msg[i]);
        MidiOut.clear();
        std::vector<std::pair<int,uint32_t> > a;
        a.push_back(std::make_pair(4, ((uint32_t)0x5000<<16)|0));  /* char far * */
        a.push_back(std::make_pair(2, (uint32_t)sizeof(msg)));     /* int len    */
        CallService(VF_MS_MIDIMSG, a);
        check("returns TRUE", reg_ax, 1);
        check("byte count forwarded", (long long)MidiOut.size(), (long long)sizeof(msg));
        bool same = (MidiOut.size()==sizeof(msg));
        for (unsigned i = 0; same && i < sizeof(msg); i++) if (MidiOut[i]!=msg[i]) same=false;
        check("bytes forwarded verbatim, in order", same, 1);
    }

    printf("== MIDI device checks ==\n");
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2, MIDITONES)); a.push_back(std::make_pair(4, 0));
      CallService(VF_MS_DEVICECHECK, a); }
    check("MIDITONES -> 0xFFFF (transmitter)", RetLong(), 0xFFFF);
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2, MIDIGETDMAIRQ)); a.push_back(std::make_pair(4, 0));
      CallService(VF_MS_DEVICECHECK, a); }
    check("MIDIGETDMAIRQ -> no DMA, no IRQ", RetLong(), 0xFFFFFFFFUL);
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2, MIDIPATCHTYPE)); a.push_back(std::make_pair(4, 0x10));
      CallService(VF_MS_DEVICECHECK, a); }
    check("MIDIPATCHTYPE OPL2 -> not understood", RetLong(), 0);

    printf("== msGlobalReset / msPreLoadPatch ==\n");
    MidiOut.clear();
    { std::vector<std::pair<int,uint32_t> > none; CallService(VF_MS_GLOBALRESET, none); }
    check("emits 16 channels x 2 controllers x 3 bytes", (long long)MidiOut.size(), 96);
    check("first message is ch0 All Notes Off", (MidiOut[0]==0xB0 && MidiOut[1]==0x7B), 1);
    check("second is ch0 Reset All Controllers", (MidiOut[3]==0xB0 && MidiOut[4]==0x79), 1);

    {   /* a non-sysex patch must be refused rather than sprayed at the synth */
        phys_writeb(PhysMake(0x5100,0), 0x20);
        MidiOut.clear();
        std::vector<std::pair<int,uint32_t> > a;
        a.push_back(std::make_pair(2, 0));                          /* patch   */
        a.push_back(std::make_pair(2, 0));                          /* channel */
        a.push_back(std::make_pair(4, ((uint32_t)0x5100<<16)|0));   /* data    */
        a.push_back(std::make_pair(4, 4));                          /* length  */
        CallService(VF_MS_PRELOADPATCH, a);
        check("non-sysex patch refused", reg_ax, 0);
        check("nothing transmitted", (long long)MidiOut.size(), 0);
        std::vector<std::pair<int,uint32_t> > b;
        b.push_back(std::make_pair(2,0)); b.push_back(std::make_pair(4,0));
        CallService(VF_MS_GETLASTERROR, b);
        check("  reported as MID_UNKNOWNPATCH", reg_ax, 2);
    }
    {   /* a sysex patch is forwarded to the external device */
        const uint8_t sx[] = { 0xF0, 0x41, 0x10, 0x42, 0xF7 };
        for (unsigned i = 0; i < sizeof(sx); i++) phys_writeb(PhysMake(0x5200,0)+i, sx[i]);
        MidiOut.clear();
        std::vector<std::pair<int,uint32_t> > a;
        a.push_back(std::make_pair(2, 0)); a.push_back(std::make_pair(2, 0));
        a.push_back(std::make_pair(4, ((uint32_t)0x5200<<16)|0));
        a.push_back(std::make_pair(4, (uint32_t)sizeof(sx)));
        CallService(VF_MS_PRELOADPATCH, a);
        check("sysex patch accepted", reg_ax, 1);
        check("sysex transmitted verbatim", (long long)MidiOut.size(), (long long)sizeof(sx));
    }

    printf("== MIDI close ==\n");
    MidiOut.clear();
    reg_bx = 0x0004; reg_cx = 2;
    INT10_VBEAI_Handler();
    check("fn4 AX", reg_ax & 0xFFFF, 0x004F);
    check("closed", vbeai_midi.opened, 0);
    check("silenced all 16 channels on close", (long long)MidiOut.size(), 48);
    MidiOut.clear();
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2, MIDITONES)); a.push_back(std::make_pair(4, 0));
      CallService(VF_MS_DEVICECHECK, a); }
    check("services inert after close", RetLong(), 0);

    /* ================= midimode ================= */

    printf("== midimode none / transmitter ==\n");
    section_midimode = "none";
    VBEAI_Setup();
    reg_bx = 0x0001; reg_cx = 0; reg_dx = 0x0002;
    INT10_VBEAI_Handler();
    check("midimode=none: no MIDI device", reg_cx, 0);
    reg_bx = 0x0001; reg_cx = 0; reg_dx = 0x0001;
    INT10_VBEAI_Handler();
    check("midimode=none: WAVE still there", reg_cx, 1);

    section_midimode = "transmitter";
    midi_present = false;
    VBEAI_Setup();
    reg_bx = 0x0001; reg_cx = 0; reg_dx = 0x0002;
    INT10_VBEAI_Handler();
    check("transmitter with no output: absent", reg_cx, 0);
    midi_present = true;
    VBEAI_Setup();
    reg_bx = 0x0001; reg_cx = 0; reg_dx = 0x0002;
    INT10_VBEAI_Handler();
    check("transmitter with an output: present", reg_cx, 2);

    /* ================= OPL FM ================= */

    printf("== midimode opl2 ==\n");
    section_midimode = "opl2";
    midi_present = false;            /* the FM device must not need [midi] */
    VBEAI_Setup();
    check("FM synthesiser active", VBEAI_FM_Active(), 1);
    check("9 two-operator voices", VBEAI_FM_TotalVoices(), 9);
    reg_bx = 0x0001; reg_cx = 0; reg_dx = 0x0002;
    INT10_VBEAI_Handler();
    check("MIDI device present without any MIDI output", reg_cx, 2);

    reg_bx = 0x0002; reg_cx = 2; reg_dx = 0x0002;
    reg_si = 0x3000; reg_di = 0x0000;
    INT10_VBEAI_Handler();
    {
        PhysPt m = PhysMake(0x3000,0) + 12;
        char chip[32]; for (int i=0;i<31;i++) chip[i]=(char)phys_readb(m+76+i); chip[31]=0;
        check("michip says Yamaha OPL2", strcmp(chip,"Yamaha OPL2")==0, 1);
        check("mifeatures: preloaded, not a transmitter", phys_readd(m+126), 0x20);
        check("miactivetones = real voice count", phys_readw(m+136), 9);
    }

    reg_bx = 0x0003; reg_cx = 2; reg_dx = 0; reg_si = 0x4000;
    INT10_VBEAI_Handler();
    check("opened", reg_ax & 0xFFFF, 0x004F);

    printf("== FM note handling ==\n");
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2, MIDITONES)); a.push_back(std::make_pair(4, 0));
      CallService(VF_MS_DEVICECHECK, a); }
    check("MIDITONES = 9 free voices", RetLong(), 9);

    {   /* note on, middle C, channel 0 */
        const uint8_t msg[] = { 0x90, 0x3C, 0x7F };
        for (unsigned i=0;i<sizeof(msg);i++) phys_writeb(PhysMake(0x5000,0)+i, msg[i]);
        OplWrites.clear();
        std::vector<std::pair<int,uint32_t> > a;
        a.push_back(std::make_pair(4, ((uint32_t)0x5000<<16)|0));
        a.push_back(std::make_pair(2, (uint32_t)sizeof(msg)));
        CallService(VF_MS_MIDIMSG, a);
    }
    check("one voice taken", VBEAI_FM_FreeVoices(), 8);
    check("key-on written to B0", (OplRegs[0xB0] & 0x20) != 0, 1);
    check("programmed the voice registers", (long long)(OplWrites.size() >= 12), 1);

    {   /* note off */
        const uint8_t msg[] = { 0x80, 0x3C, 0x00 };
        for (unsigned i=0;i<sizeof(msg);i++) phys_writeb(PhysMake(0x5010,0)+i, msg[i]);
        std::vector<std::pair<int,uint32_t> > a;
        a.push_back(std::make_pair(4, ((uint32_t)0x5010<<16)|0));
        a.push_back(std::make_pair(2, (uint32_t)sizeof(msg)));
        CallService(VF_MS_MIDIMSG, a);
    }
    check("voice released", VBEAI_FM_FreeVoices(), 9);
    check("key-on cleared", (OplRegs[0xB0] & 0x20) == 0, 1);

    {   /* ten notes on nine voices: the tenth must steal, not be dropped */
        uint8_t msg[30]; unsigned n = 0;
        for (uint8_t k = 0; k < 10; k++) {
            msg[n++] = 0x90; msg[n++] = (uint8_t)(60+k); msg[n++] = 0x7F;
        }
        for (unsigned i=0;i<n;i++) phys_writeb(PhysMake(0x5020,0)+i, msg[i]);
        std::vector<std::pair<int,uint32_t> > a;
        a.push_back(std::make_pair(4, ((uint32_t)0x5020<<16)|0));
        a.push_back(std::make_pair(2, n));
        CallService(VF_MS_MIDIMSG, a);
    }
    check("all nine voices in use", VBEAI_FM_FreeVoices(), 0);

    {   /* running status: note offs with the status byte sent once */
        uint8_t msg[21]; unsigned n = 0;
        msg[n++] = 0x80;
        for (uint8_t k = 0; k < 10; k++) { msg[n++] = (uint8_t)(60+k); msg[n++] = 0x00; }
        for (unsigned i=0;i<n;i++) phys_writeb(PhysMake(0x5040,0)+i, msg[i]);
        std::vector<std::pair<int,uint32_t> > a;
        a.push_back(std::make_pair(4, ((uint32_t)0x5040<<16)|0));
        a.push_back(std::make_pair(2, n));
        CallService(VF_MS_MIDIMSG, a);
    }
    check("running status understood, all released", VBEAI_FM_FreeVoices(), 9);

    {   /* Re-striking a key while the sustain pedal is down must reclaim the
         * old voice, not leave it pedal-held and take a second one. Getting
         * this wrong starves the chip on any pedalled piece. */
        uint8_t msg[] = { 0xB0, 64, 127,        /* sustain on            */
                          0x90, 0x40, 0x7F,     /* note on              */
                          0x80, 0x40, 0x00,     /* note off (pedal holds)*/
                          0x90, 0x40, 0x7F,     /* same note again      */
                          0x80, 0x40, 0x00 };
        for (unsigned i=0;i<sizeof(msg);i++) phys_writeb(PhysMake(0x5080,0)+i, msg[i]);
        std::vector<std::pair<int,uint32_t> > a;
        a.push_back(std::make_pair(4, ((uint32_t)0x5080<<16)|0));
        a.push_back(std::make_pair(2, (uint32_t)sizeof(msg)));
        CallService(VF_MS_MIDIMSG, a);
    }
    check("re-strike under pedal reuses one voice", VBEAI_FM_FreeVoices(), 8);
    {   /* lifting the pedal releases it */
        const uint8_t msg[] = { 0xB0, 64, 0 };
        for (unsigned i=0;i<sizeof(msg);i++) phys_writeb(PhysMake(0x5090,0)+i, msg[i]);
        std::vector<std::pair<int,uint32_t> > a;
        a.push_back(std::make_pair(4, ((uint32_t)0x5090<<16)|0));
        a.push_back(std::make_pair(2, (uint32_t)sizeof(msg)));
        CallService(VF_MS_MIDIMSG, a);
    }
    check("pedal release frees the voice", VBEAI_FM_FreeVoices(), 9);

    {   /* Pitch: decode the F-number and block actually written back into a
         * frequency and compare with equal temperament. The OPL's F-number
         * table is coarse, so allow 1%, but a wrong octave or a wrong table
         * index would be far outside that. */
        static const struct { uint8_t note; double hz; const char *name; } tones[] = {
            { 57, 220.00, "A3" }, { 60, 261.63, "C4" },
            { 69, 440.00, "A4" }, { 81, 880.00, "A5" }
        };
        bool all_ok = true;
        double worst = 0.0;
        for (unsigned t = 0; t < sizeof(tones)/sizeof(tones[0]); t++) {
            VBEAI_FM_Reset();
            const uint8_t msg[] = { 0x90, tones[t].note, 0x7F };
            for (unsigned i=0;i<sizeof(msg);i++) phys_writeb(PhysMake(0x50A0,0)+i, msg[i]);
            std::vector<std::pair<int,uint32_t> > a;
            a.push_back(std::make_pair(4, ((uint32_t)0x50A0<<16)|0));
            a.push_back(std::make_pair(2, (uint32_t)sizeof(msg)));
            CallService(VF_MS_MIDIMSG, a);

            const unsigned fnum  = OplRegs[0xA0] | ((OplRegs[0xB0] & 0x03) << 8);
            const unsigned block = (OplRegs[0xB0] >> 2) & 0x07;
            const double hz = (double)fnum * 49716.0 / (double)(1u << (20 - block));
            const double err = (hz - tones[t].hz) / tones[t].hz;
            if (err > worst || -err > worst) worst = (err < 0) ? -err : err;
            if (err > 0.0015 || err < -0.0015) all_ok = false;
            printf("        %-3s wanted %7.2f Hz, chip gives %7.2f Hz (%+.2f%%)\n",
                   tones[t].name, tones[t].hz, hz, err * 100.0);
        }
        check("every test pitch within 0.15% of equal temperament", all_ok, 1);
        printf("        worst deviation %.2f%%\n", worst * 100.0);
        VBEAI_FM_Reset();
    }

    printf("== FM device checks ==\n");
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2, MIDIPATCHTYPE));
      a.push_back(std::make_pair(4, VBEAI_PATCH_OPL2));
      CallService(VF_MS_DEVICECHECK, a); }
    check("understands the OPL2 patch type", RetLong(), 1);
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2, MIDIPATCHTYPE));
      a.push_back(std::make_pair(4, VBEAI_PATCH_OPL3));
      CallService(VF_MS_DEVICECHECK, a); }
    check("OPL3 patch type refused on an OPL2", RetLong(), 0);

    {   /* MIDIVOICESTEAL: disable stealing on channel 0, then exhaust it */
        std::vector<std::pair<int,uint32_t> > a;
        a.push_back(std::make_pair(2, MIDIVOICESTEAL));
        a.push_back(std::make_pair(4, (uint32_t)(0x0001u) << 16 | 0));  /* mask ch0, disable */
        CallService(VF_MS_DEVICECHECK, a);
        check("stealing disabled on channel 0", (VBEAI_FM_GetVoiceSteal() & 1), 0);
    }
    {
        uint8_t msg[30]; unsigned n = 0;
        for (uint8_t k = 0; k < 10; k++) { msg[n++] = 0x90; msg[n++] = (uint8_t)(60+k); msg[n++] = 0x7F; }
        for (unsigned i=0;i<n;i++) phys_writeb(PhysMake(0x5060,0)+i, msg[i]);
        std::vector<std::pair<int,uint32_t> > a;
        a.push_back(std::make_pair(4, ((uint32_t)0x5060<<16)|0));
        a.push_back(std::make_pair(2, n));
        CallService(VF_MS_MIDIMSG, a);
    }
    check("tenth note dropped rather than stealing", VBEAI_FM_FreeVoices(), 0);
    VBEAI_FM_SetVoiceSteal(0xFFFF);
    VBEAI_FM_Reset();

    printf("== msPreLoadPatch (OPL2) ==\n");
    {
        /* patchtype(2) mode(1) percVoice(1) op0(13) op1(13) wave0(1) wave1(1) */
        PhysPt p = PhysMake(0x5200,0);
        for (int i=0;i<32;i++) phys_writeb(p+i, 0);
        phys_writew(p+0, VBEAI_PATCH_OPL2);
        /* op0: ksl=2 mult=5 feedback=3 attack=12 sustLevel=4 sustain=1
         *      decay=7 release=9 output=0x1A am=1 vib=0 ksr=1 fm=1        */
        phys_writeb(p+4+0, 2); phys_writeb(p+4+1, 5);  phys_writeb(p+4+2, 3);
        phys_writeb(p+4+3, 12); phys_writeb(p+4+4, 4); phys_writeb(p+4+5, 1);
        phys_writeb(p+4+6, 7); phys_writeb(p+4+7, 9);  phys_writeb(p+4+8, 0x1A);
        phys_writeb(p+4+9, 1); phys_writeb(p+4+10, 0); phys_writeb(p+4+11, 1);
        phys_writeb(p+4+12, 1);
        phys_writeb(p+4+26, 2);                 /* wave0 */

        std::vector<std::pair<int,uint32_t> > a;
        a.push_back(std::make_pair(2, 5));                          /* program */
        a.push_back(std::make_pair(2, 0));                          /* channel */
        a.push_back(std::make_pair(4, ((uint32_t)0x5200<<16)|0));   /* data    */
        a.push_back(std::make_pair(4, 32));                         /* length  */
        CallService(VF_MS_PRELOADPATCH, a);
        check("OPL2 patch accepted", reg_ax, 1);
    }
    {
        const FMPatch &p = fm.patch[5];
        check("  AM|EG|KSR|MULT packed", p.op[0].am_vib_eg_ksr_mult, 0x80|0x20|0x10|5);
        check("  KSL|TL packed", p.op[0].ksl_tl, (2<<6)|0x1A);
        check("  attack|decay packed", p.op[0].ar_dr, (12<<4)|7);
        check("  sustain|release packed", p.op[0].sl_rr, (4<<4)|9);
        check("  waveform taken", p.op[0].waveform, 2);
        /* opl2fm=1 means frequency modulation, and the chip's connection bit
         * is 1 for additive, so it must have inverted */
        check("  feedback|connection, FM inverted", p.fb_cnt, (3<<1)|0);
    }
    {   /* a type we do not understand must be refused */
        PhysPt p = PhysMake(0x5300,0);
        phys_writew(p+0, 0x1234);
        std::vector<std::pair<int,uint32_t> > a;
        a.push_back(std::make_pair(2, 6)); a.push_back(std::make_pair(2, 0));
        a.push_back(std::make_pair(4, ((uint32_t)0x5300<<16)|0));
        a.push_back(std::make_pair(4, 32));
        CallService(VF_MS_PRELOADPATCH, a);
        check("unknown patch type refused", reg_ax, 0);
    }
    {   /* msUnloadPatch restores the built-in */
        std::vector<std::pair<int,uint32_t> > a;
        a.push_back(std::make_pair(2, 5)); a.push_back(std::make_pair(2, 0));
        CallService(VF_MS_UNLOADPATCH, a);
        check("unload restores the built-in patch",
              fm.patch[5].op[0].am_vib_eg_ksr_mult == fm_family[5/8].op[0].am_vib_eg_ksr_mult, 1);
    }

    printf("== midimode opl3 ==\n");
    section_midimode = "opl3";
    VBEAI_Setup();
    check("18 voices", VBEAI_FM_TotalVoices(), 18);
    check("OPL3 NEW bit set", OplRegs[0x105] & 1, 1);
    reg_bx = 0x0002; reg_cx = 2; reg_dx = 0x0002;
    reg_si = 0x3100; reg_di = 0x0000;
    INT10_VBEAI_Handler();
    {
        PhysPt m = PhysMake(0x3100,0) + 12;
        char chip[32]; for (int i=0;i<31;i++) chip[i]=(char)phys_readb(m+76+i); chip[31]=0;
        check("michip says Yamaha OPL3", strcmp(chip,"Yamaha OPL3")==0, 1);
        check("miactivetones = 18", phys_readw(m+136), 18);
    }
    reg_bx = 0x0003; reg_cx = 2; reg_dx = 0; reg_si = 0x4100;
    INT10_VBEAI_Handler();
    { std::vector<std::pair<int,uint32_t> > a;
      a.push_back(std::make_pair(2, MIDIPATCHTYPE));
      a.push_back(std::make_pair(4, VBEAI_PATCH_OPL3));
      CallService(VF_MS_DEVICECHECK, a); }
    check("understands the OPL3 patch type", RetLong(), 1);

    {   /* eighteen notes must all fit */
        uint8_t msg[54]; unsigned n = 0;
        for (uint8_t k = 0; k < 18; k++) { msg[n++] = 0x90; msg[n++] = (uint8_t)(48+k); msg[n++] = 0x7F; }
        for (unsigned i=0;i<n;i++) phys_writeb(PhysMake(0x5400,0)+i, msg[i]);
        OplWrites.clear();
        std::vector<std::pair<int,uint32_t> > a;
        a.push_back(std::make_pair(4, ((uint32_t)0x5400<<16)|0));
        a.push_back(std::make_pair(2, n));
        CallService(VF_MS_MIDIMSG, a);
    }
    check("all 18 voices in use", VBEAI_FM_FreeVoices(), 0);
    {   /* voices 9..17 must be programmed in the second register bank */
        bool high_bank = false;
        for (size_t i = 0; i < OplWrites.size(); i++)
            if (OplWrites[i].first >= 0x100) high_bank = true;
        check("second register bank used", high_bank, 1);
    }

    section_midimode = "auto";       /* leave the harness as it found things */
    VBEAI_Setup();

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
