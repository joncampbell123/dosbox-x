
#include "dosbox.h"
#include "logging.h"
#include "setup.h"
#include "video.h"
#include "pic.h"
#include "vga.h"
#include "inout.h"
#include "programs.h"
#include "support.h"
#include "setup.h"
#include "timer.h"
#include "mem.h"
#include "util_units.h"
#include "control.h"
#include "pc98_cg.h"
#include "pc98_dac.h"
#include "pc98_gdc.h"
#include "pc98_gdc_const.h"
#include "joystick.h"
#include "regs.h"
#include "mixer.h"
#include "callback.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <stdio.h>

using namespace std;

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

#include <map>

#include "np2glue.h"

MixerChannel *pc98_mixer = NULL;

NP2CFG pccore;

extern "C" void pcm86io_setid(unsigned int baseio);

extern unsigned char pc98_mem_msw_m[8];
bool pc98_soundbios_rom_load = true;
bool pc98_soundbios_enabled = false;

extern "C" unsigned char *CGetMemBase() {
    return MemBase;
}

extern "C" void sound_sync(void) {
    if (pc98_mixer) pc98_mixer->FillUp();
}

extern "C" void _TRACEOUT(const char *fmt,...) {
    va_list va;
    char tmp[512];

    va_start(va,fmt);

    vsnprintf(tmp,sizeof(tmp),fmt,va);
    LOG_MSG("PC98FM TRACEOUT: %s",tmp);

    va_end(va);
}

void getbiospath(OEMCHAR *path, const OEMCHAR *fname, int maxlen) {
    LOG_MSG("PC98FM getbiospath fname='%s'",fname);
    snprintf(path,(size_t)maxlen,"%s",fname);
}

enum {
	JOY_UP_BIT		    = (1u << 0u),
	JOY_DOWN_BIT		= (1u << 1u),
	JOY_LEFT_BIT		= (1u << 2u),
	JOY_RIGHT_BIT		= (1u << 3u),
	JOY_RAPIDBTN1_BIT	= (1u << 4u),
	JOY_RAPIDBTN2_BIT	= (1u << 5u),
	JOY_BTN1_BIT		= (1u << 6u),
	JOY_BTN2_BIT		= (1u << 7u)
};


REG8 joymng_getstat(void) {
    unsigned char r = 0xFF;

    if (JOYSTICK_IsEnabled(0)) {
        if (JOYSTICK_GetButton(0,0))
            r &= ~JOY_BTN1_BIT;
        if (JOYSTICK_GetButton(0,1))
            r &= ~JOY_BTN2_BIT;

        float x = JOYSTICK_GetMove_X(0);
        float y = JOYSTICK_GetMove_Y(0);

        if (x >= 0.5)
            r &= ~JOY_RIGHT_BIT;
        else if (x <= -0.5)
            r &= ~JOY_LEFT_BIT;

        if (y >= 0.5)
            r &= ~JOY_DOWN_BIT;
        else if (y <= -0.5)
            r &= ~JOY_UP_BIT;
    }

    return r;
}

REG8 keystat_getjoy(void) {
    return 0xFF;
}

struct CBUS4PORT {
    IOINP       inp;
    IOOUT       out;

    CBUS4PORT() : inp(NULL), out(NULL) { }
};

static std::map<UINT,CBUS4PORT> cbuscore_map;

void pc98_fm86_write(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    const auto &cbusm = cbuscore_map[port];
    const auto &func = cbusm.out;
    if (func) func(port,val);
}

Bitu pc98_fm86_read(Bitu port,Bitu iolen) {
    (void)iolen;//UNUSED
    const auto &cbusm = cbuscore_map[port];
    const auto &func = cbusm.inp;
    if (func) return func(port);
    return ~0ul;
}

// four I/O ports, 2 ports apart
void cbuscore_attachsndex(UINT port, const IOOUT *out, const IOINP *inp) {
    LOG_MSG("cbuscore_attachsndex(port=0x%x)",port);

    for (unsigned int i=0;i < 4;i++) {
        auto &cbusm = cbuscore_map[port+(i*2)];

        IO_RegisterReadHandler(port+(i*2),pc98_fm86_read,IO_MB);
        cbusm.inp = inp[i];

        IO_RegisterWriteHandler(port+(i*2),pc98_fm86_write,IO_MB);
        cbusm.out = out[i];
    }
}

void pic_setirq(REG8 irq) {
    PIC_ActivateIRQ(irq);
}

void pic_resetirq(REG8 irq) {
    PIC_DeActivateIRQ(irq);
}

#include "sound.h"
#include "fmboard.h"

static void avsdrv_check_size(SINT32 size);

// wrapper for fmtimer events
void fmport_a(NEVENTITEM item);
void fmport_b(NEVENTITEM item);

static void fmport_a_pic_event(Bitu val) {
    (void)val;//UNUSED
    fmport_a(NULL);
}

static void fmport_b_pic_event(Bitu val) {
    (void)val;//UNUSED
    fmport_b(NULL);
}

// save state support
void *fmport_a_pic_event_PIC_Event = (void*)((uintptr_t)fmport_a_pic_event);
void *fmport_b_pic_event_PIC_Event = (void*)((uintptr_t)fmport_b_pic_event);

extern "C" void pc98_fm_dosbox_fmtimer_setevent(unsigned int n,double dt) {
    PIC_EventHandler func = n ? fmport_b_pic_event : fmport_a_pic_event;

    PIC_RemoveEvents(func);
    PIC_AddEvent(func,dt);
}

extern "C" void pc98_fm_dosbox_fmtimer_clearevent(unsigned int n) {
    PIC_RemoveEvents(n ? fmport_b_pic_event : fmport_a_pic_event);
}

// mixer callback

static void pc98_mix_CallBack(Bitu len) {
    unsigned int s = len;

    if (s > (sizeof(MixTemp)/sizeof(int32_t)/2))
        s = (sizeof(MixTemp)/sizeof(int32_t)/2);

    memset(MixTemp,0,s * sizeof(int32_t) * 2);

    opngen_getpcm(NULL, (SINT32*)MixTemp, s);
    tms3631_getpcm(&tms3631, (SINT32*)MixTemp, s);

    for (unsigned int i=0;i < 3;i++)
        psggen_getpcm(&__psg[i], (SINT32*)MixTemp, s);
 
    // NTS: _RHYTHM is a struct with the same initial layout as PCMMIX
    pcmmix_getpcm((PCMMIX)(&rhythm), (SINT32*)MixTemp, s);
#if defined(SUPPORT_PX)
    pcmmix_getpcm((PCMMIX)(&rhythm2), (SINT32*)MixTemp, s);
    pcmmix_getpcm((PCMMIX)(&rhythm3), (SINT32*)MixTemp, s);
#endif	// defined(SUPPORT_PX)

    adpcm_getpcm(&adpcm, (SINT32*)MixTemp, s);
#if defined(SUPPORT_PX)
    adpcm_getpcm(&adpcm2, (SINT32*)MixTemp, s);
    adpcm_getpcm(&adpcm3, (SINT32*)MixTemp, s);
#endif	// defined(SUPPORT_PX)

    SINT32 start = pcm86.realbuf;
    pcm86gen_getpcm(NULL, (SINT32*)MixTemp, s);
	avsdrv_check_size(start - pcm86.realbuf);

    pc98_mixer->AddSamples_s32(s, (int32_t*)MixTemp);
}

static bool pc98fm_init = false;

extern "C" {
UINT8 fmtimer_irq2index(const UINT8 irq);
UINT8 fmtimer_index2irq(const UINT8 index);
void fmboard_on_reset();
void rhythm_deinitialize(void);
}

UINT8 board86_encodeirqidx(const unsigned char idx) {
    /* see board86.c to understand what this is about */
    return  ((idx & 1) ? 0x08 : 0x00) +
            ((idx & 2) ? 0x04 : 0x00);
}

UINT8 board26k_encodeirqidx(const unsigned char idx) {
    /* see board26k.c to understand what this is about */
    return  (idx << 6);
}

void PC98_FM_Destroy(Section *sec) {
    (void)sec;//UNUSED
    if (pc98fm_init) {
        rhythm_deinitialize();
    }

    if (pc98_mixer) {
        MIXER_DelChannel(pc98_mixer);
        pc98_mixer = NULL;
    }
}

void pc98_set_msw4_soundbios(void)
{
    /* Set MSW4 bit 3 for sound BIOS. */
    if(pc98_soundbios_enabled)
        pc98_mem_msw_m[3/*MSW4*/] |= 0x8;
    else
        pc98_mem_msw_m[3/*MSW4*/] &= ~((unsigned char)0x8);
}

static CALLBACK_HandlerObject soundbios_callback;

// for more information see chapter 13 of [https://ia801305.us.archive.org/8/items/PC9800TechnicalDataBookBIOS1992/PC-9800TechnicalDataBook_BIOS_1992_text.pdf]
static Bitu SOUNDROM_INTD2_PC98_Handler(void) {
    const char *call_name = "?";

    switch (reg_ah) {
        case 0x00:  // INITIALIZE
            call_name = "INITIALIZE";
            goto unknown;
        case 0x01:  // PLAY
            call_name = "PLAY";
            goto unknown;
        case 0x02:  // CLEAR
            call_name = "CLEAR";
            goto unknown;
        case 0x10:  // READ REG
            call_name = "READ REG";
            goto unknown;
        case 0x11:  // WRITE REG
            call_name = "WRITE REG";
            goto unknown;
        case 0x12:  // SET TOUCH
            call_name = "SET TOUCH";
            goto unknown;
        case 0x13:  // NOTE
            call_name = "NOTE";
            goto unknown;
        case 0x14:  // SET LENGTH
            call_name = "SET LENGTH";
            goto unknown;
        case 0x15:  // SET TEMPO
            call_name = "SET TEMPO";
            goto unknown;
        case 0x16:  // SET PARA BLOCK
            call_name = "SET PARA BLOCK";
            goto unknown;
        case 0x17:  // READ PARA
            call_name = "READ PARA";
            goto unknown;
        case 0x18:  // WRITE PARA
            call_name = "WRITE PARA";
            goto unknown;
        case 0x19:  // ALL STOP
            call_name = "ALL STOP";
            goto unknown;
        case 0x1A:  // CONT PLAY
            call_name = "CONT PLAY";
            goto unknown;
        case 0x1B:  // MODU ON
            call_name = "MODU ON";
            goto unknown;
        case 0x1C:  // MODU OFF
            call_name = "MODU OFF";
            goto unknown;
        case 0x1D:  // SET INT COND
            call_name = "SET INT COND";
            goto unknown;
        case 0x1E:  // HOLD STATE
            call_name = "HOLD STATE";
            goto unknown;
        case 0x1F:  // SET VOLUME
            call_name = "SET VOLUME";
            goto unknown;
        default:
        unknown:
            LOG_MSG("PC-98 SOUND BIOS (INT D2h) call '%s' with AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
                    call_name,
                    reg_ax,
                    reg_bx,
                    reg_cx,
                    reg_dx,
                    reg_si,
                    reg_di,
                    SegValue(ds),
                    SegValue(es));
            break;
    }

    // guessing, from Yksoft1's patch
    reg_ah = 0;

    return CBRET_NONE;
}

bool LoadSoundBIOS(void) {
    FILE *fp;

    if (!pc98_soundbios_rom_load) return false;

    fp = fopen("SOUND.ROM","rb");
    if (!fp) fp = fopen("sound.rom","rb");
    if (!fp) return false;

    if (fread(MemBase+0xCC000,0x4000,1,fp) != 1) {
        LOG_MSG("PC-98 SOUND.ROM failed to read 16k");
        fclose(fp);
        return false;
    }

    LOG_MSG("PC-98 SOUND.ROM loaded into memory");
    fclose(fp);
    return true;
}

bool PC98_FM_SoundBios_Enabled(void) {
    return pc98_soundbios_enabled;
}

void PC98_FM_OnEnterPC98(Section *sec) {
    (void)sec;//UNUSED
    Section_prop * pc98_section=static_cast<Section_prop *>(control->GetSection("pc98"));
	assert(pc98_section != NULL);
    bool was_pc98fm_init = pc98fm_init;

    if (!pc98fm_init) {
        unsigned char fmirqidx;
        unsigned int baseio;
        std::string board;
        int irq;

        soundbios_callback.Uninstall();

        board = pc98_section->Get_string("pc-98 fm board");
        if (board == "off" || board == "false") {
            /* Don't enable Sound BIOS if sound board itself is disabled. */
            pc98_soundbios_enabled = false;
            pc98_set_msw4_soundbios();
            return;		
        }

        irq = pc98_section->Get_int("pc-98 fm board irq");
        baseio = (unsigned int)pc98_section->Get_hex("pc-98 fm board io port");

        pc98_soundbios_enabled = pc98_section->Get_bool("pc-98 sound bios");
        pc98_soundbios_rom_load = pc98_section->Get_bool("pc-98 load sound bios rom file");
        pc98_set_msw4_soundbios();

        if (pc98_soundbios_enabled) {
            /* TODO: Load SOUND.ROM to CC000h - CFFFFh when Sound BIOS is enabled? 
             * Or simulate Sound BIOS calls ourselves? */
            if (LoadSoundBIOS()) {
                /* good! */
            }
            else {
                soundbios_callback.Install(&SOUNDROM_INTD2_PC98_Handler,CB_IRET,"Sound ROM INT D2h");

                /* fake INT D2h sound BIOS entry point at CEE0:0000.
                 * Unlike the LIO interface there's only one interrupt (if Neko Project II code is correct) */
                uint32_t ofs = 0xCEE0u << 4u;
                uint32_t callback_addr = soundbios_callback.Get_RealPointer();

                phys_writed(ofs+0,0x0001);      // number of entries
                phys_writew(ofs+4,0xD2);        // INT D2h entry point
                phys_writew(ofs+6,0x08);
                phys_writeb(ofs+8,0xEA);        // JMP FAR <callback>
                phys_writed(ofs+9,callback_addr);
            }
        }

        /* Manual testing shows PC-98 games like it when the board is on IRQ 12 */
        if (irq == 0) irq = 12;

        fmirqidx = fmtimer_irq2index(irq);

        pc98fm_init = true;

        unsigned int rate = 44100;

        memset(&pccore,0,sizeof(pccore));
        pccore.samplingrate = 44100;
		pccore.baseclock = PIT_TICK_RATE;
		pccore.multiple = 1;
        for (unsigned int i=0;i < 6;i++) pccore.vol14[i] = 0xFF;
        pccore.vol_fm = 128;
        pccore.vol_ssg = 128;
        pccore.vol_adpcm = 128;
        pccore.vol_pcm = 128;
        pccore.vol_rhythm = 128;

        //	fddmtrsnd_initialize(rate);
        //	beep_initialize(rate);
        //	beep_setvol(3);
        tms3631_initialize(rate);
        tms3631_setvol(pccore.vol14);
        opngen_initialize(rate);
        opngen_setvol(pccore.vol_fm);
        psggen_initialize(rate);
        psggen_setvol(pccore.vol_ssg);
        rhythm_initialize(rate);
        rhythm_setvol(pccore.vol_rhythm);
        adpcm_initialize(rate);
        adpcm_setvol(pccore.vol_adpcm);
        pcm86gen_initialize(rate);
        pcm86gen_setvol(pccore.vol_pcm);

        if (board == "board86c" || board == "auto") {
            if (baseio == 0 || baseio == 0x188) { /* default */
                pccore.snd86opt |= 0x01;
                baseio = 0x188;
            }
            else {
                baseio = 0x288;
            }

            pccore.snd86opt += board86_encodeirqidx(fmirqidx);

            LOG_MSG("PC-98 FM board is PC-9801-86c at baseio=0x%x irq=%d",baseio,fmtimer_index2irq(fmirqidx));
            fmboard_reset(&np2cfg, 0x14);
        }
        else if (board == "board86") {
            if (baseio == 0 || baseio == 0x188) { /* default */
                pccore.snd86opt |= 0x01;
                baseio = 0x188;
            }
            else {
                baseio = 0x288;
            }

            pccore.snd86opt += board86_encodeirqidx(fmirqidx);

            LOG_MSG("PC-98 FM board is PC-9801-86 at baseio=0x%x irq=%d",baseio,fmtimer_index2irq(fmirqidx));
            fmboard_reset(&np2cfg, 0x04);
        }
        else if (board == "board26k") {
            if (baseio == 0x188) {
                pccore.snd26opt |= 0x10;
                baseio = 0x188;
            }
            else { /* default */
                baseio = 0x088;
            }

            pccore.snd26opt += board26k_encodeirqidx(fmirqidx);

            LOG_MSG("PC-98 FM board is PC-9801-26k at baseio=0x%x irq=%d",baseio,fmtimer_index2irq(fmirqidx));
            fmboard_reset(&np2cfg, 0x02);
        }
        else if (board == "board14") {
            /* Apparently board14 is always IRQ 12, port 88h */
            LOG_MSG("PC-98 FM board is PC-9801-14 at baseio=0x%x irq=%d",0x88,12);
            LOG_MSG("WARNING: This is not yet implemented!"); // board14 emulation requires emulation of a PIT timer (8253) on the board itself
            fmboard_reset(&np2cfg, 0x01);
        }
        else {
            if (baseio == 0 || baseio == 0x188) { /* default */
                pccore.snd86opt |= 0x01;
                baseio = 0x188;
            }
            else {
                baseio = 0x288;
            }

            pccore.snd86opt += board86_encodeirqidx(fmirqidx);

            LOG_MSG("PC-98 FM board is PC-9801-86c at baseio=0x%x irq=%d",baseio,fmtimer_index2irq(fmirqidx));
            fmboard_reset(&np2cfg, 0x14);   // board86c, a good default
        }

        fmboard_bind();
        // Set sound ID
        pcm86io_setid(baseio);

        // WARNING: Some parts of the borrowed code assume 44100, 22050, or 11025 and
        //          will misrender if given any other sample rate (especially the OPNA synth).

        pc98_mixer = MIXER_AddChannel(pc98_mix_CallBack, rate, "PC-98");
        pc98_mixer->Enable(true);
    }

    if (was_pc98fm_init) {
        fmboard_on_reset();
        fmboard_bind(); // FIXME: Re-binds I/O ports as well
    }
}

extern "C" void pcm86io_setfreq(unsigned char val);
extern "C" void pcm86io_setvol(unsigned char val);
extern "C" void pcm86io_outpcm(unsigned char val);
extern "C" void pcm86io_setpcm(unsigned char val);

#define AVSDRV_PCB_SIZE		0x10
#define AVSDRV_PCB_MASK		0x0f
#define	AVSDRV_VOLUME_SIZE	4
#define	AVSDRV_PCM_VOLUME	2
#define	AVSDRV_OUTPUT_SIZE	1024
struct AVSDRV_PCB {
	uint16_t seg, off;
	uint16_t data_seg, data_off;
	uint16_t size, use_size;
	uint16_t pos;
	uint8_t *buffer;
	uint8_t status;
};
static AVSDRV_PCB pcb_data[AVSDRV_PCB_SIZE];
static uint16_t avsdrv_ems_pageframe;
static uint16_t pcb_write, pcb_read;
static uint8_t avsdrv_pcm;
static uint8_t avsdrv_freq;
static uint8_t avsdrv_mute;
static uint8_t avsdrv_volume[AVSDRV_VOLUME_SIZE];
static bool avsdrv_play;

Bitu GetEMSPageFrameSegment(void);

Bitu PC98_AVSDRV_PCM_Handler(void)
{
	const char *call_name = "?";

	switch(reg_ah) {
	case 0x00:
		// $INITDRV
		memset(pcb_data, 0, sizeof(pcb_data));
		pcb_write = pcb_read = 0;
		avsdrv_volume[AVSDRV_PCM_VOLUME] = 0x0a;
		pcm86io_setvol(avsdrv_volume[AVSDRV_PCM_VOLUME]);
		avsdrv_pcm = 0xa0;
		avsdrv_freq = 0;
		avsdrv_mute = 0;
		avsdrv_ems_pageframe = GetEMSPageFrameSegment();
		break;
	case 0x01:
		call_name = "$INITFUNC";
        goto unknown;
	case 0x02:
		call_name = "$ASKFUNC";
        goto unknown;
	case 0x03:
		call_name = "$SETPARAM";
        goto unknown;
	case 0x04:
		call_name = "$GETPARAM";
        goto unknown;
	case 0x05:
		// $SETVOL
		{
			uint8_t select = reg_al >> 4;
			if(select == AVSDRV_PCM_VOLUME) {
				pcm86io_setvol(reg_al);
			}
			if(select < AVSDRV_VOLUME_SIZE) {
				avsdrv_volume[select] = reg_al & 0x0f;
			}
		}
		break;
	case 0x06:
		// $GETVOL
		{
			uint8_t select = reg_al >> 4;
			if(select < AVSDRV_VOLUME_SIZE) {
				reg_al = (reg_al & 0xf0) | avsdrv_volume[select];
			}
		}
        goto unknown;
	case 0x07:
		call_name = "$SETMIX";
        goto unknown;
	case 0x08:
		call_name = "$GETMIX";
        goto unknown;
	case 0x09:
		// $SETMUTE
		avsdrv_mute = reg_al;
		if(reg_al == 1) {
			pcm86io_setvol(0);
		} else if(reg_al == 0) {
			pcm86io_setvol(avsdrv_volume[AVSDRV_PCM_VOLUME]);
		}
		break;
	case 0x0a:
		// $GETMUTE
		reg_al = avsdrv_mute;
		break;
	case 0x0b:
		// $SETFREQ
		if(avsdrv_freq != reg_al) {
			avsdrv_freq = reg_al;
			pcm86io_setfreq(avsdrv_freq);
		}
		break;
	case 0x0c:
		// $GETFREQ
		reg_al = avsdrv_freq;
		break;
	case 0x0d:
		// $SETPCM
		if(avsdrv_pcm != reg_al) {
			avsdrv_pcm = reg_al;
			pcm86io_setpcm(avsdrv_pcm);
		}
		break;
	case 0x0e:
		// $GETPCM
		reg_al = avsdrv_pcm;
		break;
	case 0x10:
		// $PCMTRN
		{
			pcm86.fifo |= 0xa0;
			avsdrv_play = true;

		    pcb_data[pcb_write].seg = SegValue(es);
		    pcb_data[pcb_write].off = reg_bx;
			pcb_data[pcb_write].size = real_readw(pcb_data[pcb_write].seg, pcb_data[pcb_write].off + 0x0a);
			pcb_data[pcb_write].data_off = real_readw(pcb_data[pcb_write].seg, pcb_data[pcb_write].off + 0x06);
			if(real_readb(pcb_data[pcb_write].seg, pcb_data[pcb_write].off + 2) == 0x01) {
				pcb_data[pcb_write].data_seg = avsdrv_ems_pageframe;
				pcb_data[pcb_write].buffer = new uint8_t[pcb_data[pcb_write].size];
				MEM_BlockRead(PhysMake(pcb_data[pcb_write].data_seg, pcb_data[pcb_write].data_off), pcb_data[pcb_write].buffer, pcb_data[pcb_write].size);
			} else {
				pcb_data[pcb_write].data_seg = real_readw(pcb_data[pcb_write].seg, pcb_data[pcb_write].off + 0x08);
				pcb_data[pcb_write].buffer = NULL;
			}
			pcb_data[pcb_write].use_size = 0;

			uint8_t value = real_readb(pcb_data[pcb_write].seg, pcb_data[pcb_write].off);
			if(avsdrv_pcm != value && value != 0xff) {
				avsdrv_pcm = value;
				pcm86io_setpcm(avsdrv_pcm);
			}
			value = real_readb(pcb_data[pcb_write].seg, pcb_data[pcb_write].off + 0x01);
			if(avsdrv_freq != value && value != 0xff) {
				avsdrv_freq = value;
				pcm86io_setfreq(avsdrv_freq);
			}
			real_writew(pcb_data[pcb_write].seg, pcb_data[pcb_write].off + 0x10, pcb_data[pcb_write].data_seg);
			real_writew(pcb_data[pcb_write].seg, pcb_data[pcb_write].off + 0x0e, pcb_data[pcb_write].data_off);
		    uint8_t data[2];
		    uint16_t step = (avsdrv_pcm & 0x80) ? 2 : 1;
		    pcb_data[pcb_write].pos = pcb_data[pcb_write].data_off;
			for(uint16_t len = 0 ; len < pcb_data[pcb_write].size ; len += step) {
				if(pcm86.realbuf >= PCM86_REALBUFSIZE - 4 || (len >= AVSDRV_OUTPUT_SIZE && pcb_data[pcb_write].buffer == NULL)) {
					break;
				}
				if(pcb_data[pcb_write].buffer) {
					data[0] = pcb_data[pcb_write].buffer[pcb_data[pcb_write].pos++];
				} else {
					data[0] = real_readb(pcb_data[pcb_write].data_seg, pcb_data[pcb_write].pos++);
				}
				pcm86io_outpcm(data[0]);
				if(step == 2) {
					// 16bit
					if(pcb_data[pcb_write].buffer) {
						data[1] = pcb_data[pcb_write].buffer[pcb_data[pcb_write].pos++];
					} else {
						data[1] = real_readb(pcb_data[pcb_write].data_seg, pcb_data[pcb_write].pos++);
					}
					pcm86io_outpcm(data[1]);
				}
				if((avsdrv_pcm & 0x70) != 0x20) {
					// mono -> stereo
					pcm86io_outpcm(data[0]);
					if(step == 2) {
						// 16bit
						pcm86io_outpcm(data[1]);
					}
				}
			}
			real_writew(pcb_data[pcb_write].seg, pcb_data[pcb_write].off + 0x12, 0);
			real_writew(pcb_data[pcb_write].seg, pcb_data[pcb_write].off + 0x14, 0);
			real_writew(pcb_data[pcb_write].seg, pcb_data[pcb_write].off + 0x03, 0xff);
			pcb_data[pcb_write].status = 0xff;
			pcb_write++;
			pcb_write &= AVSDRV_PCB_MASK;
		}
		break;
	case 0x11:
		// $STPTRN
		while(pcb_write != pcb_read) {
			real_writeb(pcb_data[pcb_read].seg, pcb_data[pcb_read].off + 0x03, 0x01);
			pcb_data[pcb_read].status = 0x01;
			pcb_read++;
			pcb_read &= AVSDRV_PCB_MASK;
		}
		if(pcb_data[pcb_read].seg != 0) {
			real_writeb(pcb_data[pcb_read].seg, pcb_data[pcb_read].off + 0x03, 0x01);
			pcb_data[pcb_read].status = 0x01;
		}
		pcm86.readpos = pcm86.wrtpos;
		pcm86.realbuf = 0;
		pcm86.fifo &= ~0xa0;
		break;
	case 0x12:
		// $SNSTRN
		reg_al = ((avsdrv_pcm & 0x0f) << 4) | 0x01;
		if(pcb_write == pcb_read) {
			// POLICENAUTS
			// If you don't do this, the audio will be cut off for a very short time.
			if(pcb_data[pcb_read].use_size < 2048 || pcm86.realbuf == 0) {
				reg_al &= ~0x01;
			}
		}
		SegSet16(es, pcb_data[pcb_read].seg);
		reg_bx = pcb_data[pcb_read].off;
		reg_cx = 0;
		reg_dx = pcm86.realbuf;
		if((avsdrv_pcm & 0x70) != 0x20) {
			reg_dx /= 2;
		}
		break;
	default:
	unknown:
		LOG_MSG("PC-98 AVSDRV.SYS PCM (INT D9h) call '%s' with AX=%04X ES=%04X BX=%04X", call_name, reg_ax, SegValue(es), reg_bx);
		break;
	}
	CALLBACK_SCF(false);
	return CBRET_NONE;
}

static void avsdrv_check_size(SINT32 size)
{
	if(pcb_data[pcb_read].status == 0xff) {
		if((avsdrv_pcm & 0x70) != 0x20) {
			size /= 2;
		}
		pcb_data[pcb_read].use_size += size;

		// POLICENAUTS
        // Dealing with the interruption of explosion sounds, etc.
		if(pcb_data[pcb_read].use_size > (pcb_data[pcb_read].pos - pcb_data[pcb_read].data_off) / 2) {
		    uint8_t data[2];
		    uint16_t step = (avsdrv_pcm & 0x80) ? 2 : 1;
		    uint16_t data_end = pcb_data[pcb_read].data_off + pcb_data[pcb_read].size;
			for(uint16_t len = 0 ; len < AVSDRV_OUTPUT_SIZE && pcb_data[pcb_read].pos < data_end ; len += step) {
				if(pcm86.realbuf >= PCM86_REALBUFSIZE - 4) {
					break;
				}
				if(pcb_data[pcb_read].buffer) {
					data[0] = pcb_data[pcb_read].buffer[pcb_data[pcb_read].pos++];
				} else {
					data[0] = real_readb(pcb_data[pcb_read].data_seg, pcb_data[pcb_read].pos++);
				}
				pcm86io_outpcm(data[0]);
				if(step == 2) {
					if(pcb_data[pcb_read].buffer) {
						data[1] = pcb_data[pcb_read].buffer[pcb_data[pcb_read].pos++];
					} else {
						data[1] = real_readb(pcb_data[pcb_read].data_seg, pcb_data[pcb_read].pos++);
					}
					pcm86io_outpcm(data[1]);
				}
				if((avsdrv_pcm & 0x70) != 0x20) {
					pcm86io_outpcm(data[0]);
					if(step == 2) {
						pcm86io_outpcm(data[1]);
					}
				}
			}
		}
		// POLICENAUTS
		// It has to be in units of 2048 bytes to lip-syncs.
		uint16_t use_size = pcb_data[pcb_read].use_size & 0xf800;
		if(pcb_data[pcb_read].use_size >= pcb_data[pcb_read].size) {
			size = pcb_data[pcb_read].use_size - pcb_data[pcb_read].size;
			real_writeb(pcb_data[pcb_read].seg, pcb_data[pcb_read].off + 0x03, 0);
			pcb_data[pcb_read].status = 0x00;
			real_writew(pcb_data[pcb_read].seg, pcb_data[pcb_read].off + 0x12, use_size);
			real_writew(pcb_data[pcb_read].seg, pcb_data[pcb_read].off + 0x0e, pcb_data[pcb_read].data_off + use_size);
			if(pcb_data[pcb_read].buffer) {
				delete [] pcb_data[pcb_read].buffer;
				pcb_data[pcb_read].buffer = NULL;
			}
			pcb_read++;
			pcb_read &= AVSDRV_PCB_MASK;

			pcb_data[pcb_read].use_size += size;

		} else {
			real_writew(pcb_data[pcb_read].seg, pcb_data[pcb_read].off + 0x12, use_size);
			real_writew(pcb_data[pcb_read].seg, pcb_data[pcb_read].off + 0x0e, pcb_data[pcb_read].data_off + use_size);
		}
	}
	if(avsdrv_play && pcm86.realbuf == 0) {
		avsdrv_play = false;
		pcm86.fifo &= ~0xa0;
	} else {
		if(pcb_read != pcb_write && pcm86.realbuf != 0 && (pcm86.fifo & 0x80) == 0) {
			avsdrv_play = true;
			pcm86.fifo |= 0x80;
		}
	}
}

