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
#include "logging.h"

/* ---- stub state ---- */
uint8_t GuestMem[0x110000];
RegFile R;
CPUBlock cpu;
bool log_verbose = false;
Config* control;
MIXER_Handler TheMixerHandler = NULL;

static MixerChannel TheChannel;
static Section_prop TheSection;
static bool section_enable = true;

bool Section_prop::Get_bool(const char*) const { return section_enable; }
Section* Config::GetSection(const char*) const { return (Section*)&TheSection; }

static Bitu rom_next = 0xF8000;
Bitu ROMBIOS_GetMemory(Bitu bytes,const char*,Bitu,Bitu){ Bitu r=rom_next; rom_next+=bytes; return r; }

uint8_t CALLBACK_Allocate(){ return 42; }
bool CALLBACK_Setup(Bitu,CallBack_Handler,Bitu,const char*){ return true; }

MixerChannel* MIXER_AddChannel(MIXER_Handler h,Bitu f,const char*){
    TheMixerHandler=h; TheChannel.freq=f; return &TheChannel;
}

/* ---- the code under test ---- */
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
    reg_bx = 0x0001; reg_cx = 0; reg_dx = 0x0002;   /* MIDI */
    INT10_VBEAI_Handler();
    check("fn1 MIDI class returns 0", reg_cx, 0);

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
        for (int i = 0; i < VF_SYNCRET; i++)
            if (phys_readd(s+24+i*4) != (uint32_t)VBEAI_StubPtr(i)) ptrs_ok = false;
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

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
