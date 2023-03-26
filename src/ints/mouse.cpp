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
 *  VMware mouse protocol support by the DOSBox Staging Team (ported by Wengier)
 */

/* TODO: If biosps2=true and aux=false, also allow an option (default disabled)
 *       where if set, we don't bother to fire IRQ 12 at all but simply call the
 *       device callback directly. */

#include <string.h>
#include <math.h>
#include <vector>

#include "dosbox.h"
#include "callback.h"
#include "logging.h"
#include "mem.h"
#include "regs.h"
#include "cpu.h"
#include "mouse.h"
#include "pic.h"
#include "inout.h"
#include "int10.h"
#include "bios.h"
#include "jfont.h"
#include "dos_inc.h"
#include "support.h"
#include "setup.h"
#include "control.h"
#include "SDL.h"

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

#define VMWARE_PORT         0x5658u        // communication port
#define VMWARE_PORTHB       0x5659u        // communication port, high bandwidth
static Bitu PortRead(Bitu port, Bitu iolen);

static constexpr uint32_t TYPE_STANDARD     = 0;
static constexpr uint32_t TYPE_INTELLIMOUSE = 3;

static constexpr uint32_t RATE_10  = 0; // sample rate 10 Hz
static constexpr uint32_t RATE_20  = 1;
static constexpr uint32_t RATE_40  = 2;
static constexpr uint32_t RATE_60  = 3;
static constexpr uint32_t RATE_80  = 4;
static constexpr uint32_t RATE_100 = 5;
static constexpr uint32_t RATE_200 = 6;

// Unlock sequences taken from https://www.scs.stanford.edu/10wi-cs140/pintos/specs/kbd/scancodes-12.html
static const std::vector<uint32_t> UNLOCK_SEQ_INTELLIMOUSE = { RATE_200, RATE_100, RATE_80 };

/* ints/bios.cpp */
void bios_enable_ps2();
void WriteCharTopView(uint16_t off, int count);
uint16_t GetTextSeg();

/* hardware/keyboard.cpp */
void AUX_INT33_Takeover();
int KEYBOARD_AUX_Active();
void KEYBOARD_AUX_Event(float x,float y,Bitu buttons,int scrollwheel);
extern bool MOUSE_IsLocked();
extern bool usesystemcursor, dbcs_sbcs, showdbcs, del_flag;

bool en_int33=false;
bool en_vmware=false;
bool en_bios_ps2mouse=false;
bool cell_granularity_disable=false;
bool en_int33_hide_if_polling=false;
bool en_int33_hide_if_intsub=false;
bool en_int33_pc98_show_graphics=true; // NEC MOUSE.COM behavior

double int33_last_poll = 0;

void DisableINT33() {
    if (en_int33) {
        LOG(LOG_MOUSE, LOG_DEBUG)("Disabling INT 33 services");

        en_int33 = false;
        /* TODO: Also unregister INT 33h handler */
    }
}

void CALLBACK_DeAllocate(Bitu in);

static Bitu int74_ret_callback = 0;
static Bitu call_mouse_bd = 0;
static Bitu call_int33 = 0;
static Bitu call_int74 = 0;
static Bitu call_ps2, call_uir = 0;

void MOUSE_Unsetup_DOS(void) {
    if (call_mouse_bd != 0) {
        CALLBACK_DeAllocate(call_mouse_bd);
        call_mouse_bd = 0;
    }
    if (call_int33 != 0) {
        CALLBACK_DeAllocate(call_int33);
        call_int33 = 0;
    }
}

void MOUSE_Unsetup_BIOS(void) {
    if (int74_ret_callback != 0) {
        CALLBACK_DeAllocate(int74_ret_callback);
        int74_ret_callback = 0;
    }
    if (call_int74 != 0) {
        CALLBACK_DeAllocate(call_int74);
        call_int74 = 0;
    }
    if (call_ps2 != 0) {
        CALLBACK_DeAllocate(call_ps2);
        call_ps2 = 0;
    }
}

static uint16_t ps2cbseg,ps2cbofs;
static bool useps2callback,ps2callbackinit;
static RealPt ps2_callback,uir_callback;
static int16_t oldmouseX, oldmouseY;

// serial mouse emulation
void on_mouse_event_for_serial(int delta_x,int delta_y,uint8_t buttonstate);

struct button_event {
    uint8_t type;
    uint8_t buttons;
};

extern bool enable_slave_pic, rtl;
extern uint8_t p7fd8_8255_mouse_int_enable;

uint8_t MOUSE_IRQ = 12; // IBM PC/AT default

#define QUEUE_SIZE 32
#define MOUSE_BUTTONS 3
#define POS_X (static_cast<int16_t>(mouse.x) & mouse.gran_x)
#define POS_Y (static_cast<int16_t>(mouse.y) & mouse.gran_y)

#define CURSORX 16
#define CURSORY 16
#define HIGHESTBIT (1<<(CURSORX-1))

static uint16_t defaultTextAndMask = 0x77FF;
static uint16_t defaultTextXorMask = 0x7700;

static uint16_t defaultScreenMask[CURSORY] = {
        0x3FFF, 0x1FFF, 0x0FFF, 0x07FF,
        0x03FF, 0x01FF, 0x00FF, 0x007F,
        0x003F, 0x001F, 0x01FF, 0x00FF,
        0x30FF, 0xF87F, 0xF87F, 0xFCFF
};

static uint16_t defaultCursorMask[CURSORY] = {
        0x0000, 0x4000, 0x6000, 0x7000,
        0x7800, 0x7C00, 0x7E00, 0x7F00,
        0x7F80, 0x7C00, 0x6C00, 0x4600,
        0x0600, 0x0300, 0x0300, 0x0000
};

static uint16_t userdefScreenMask[CURSORY];
static uint16_t userdefCursorMask[CURSORY];

static struct {
    uint8_t buttons;
    int16_t wheel;
    uint16_t times_pressed[MOUSE_BUTTONS];
    uint16_t times_released[MOUSE_BUTTONS];
    uint16_t last_released_x[MOUSE_BUTTONS];
    uint16_t last_released_y[MOUSE_BUTTONS];
    uint16_t last_pressed_x[MOUSE_BUTTONS];
    uint16_t last_pressed_y[MOUSE_BUTTONS];
    pic_tickindex_t hidden_at;
    uint16_t last_scrolled_x;
    uint16_t last_scrolled_y;
    uint16_t hidden;
    float add_x,add_y;
    int16_t min_x,max_x,min_y,max_y;
    int16_t max_screen_x,max_screen_y;
    float mickey_x,mickey_y;
    float x,y;
    float ps2x,ps2y;
    button_event event_queue[QUEUE_SIZE];
    uint8_t events;//Increase if QUEUE_SIZE >255 (currently 32)
    uint16_t sub_seg,sub_ofs;
    uint16_t sub_mask;

    bool    background;
    int16_t  backposx, backposy;
    uint8_t   backData[CURSORX*CURSORY];
    uint16_t* screenMask;
    uint16_t* cursorMask;
    int16_t  clipx,clipy;
    int16_t  hotx,hoty;
    uint16_t  textAndMask, textXorMask;

    float   mickeysPerPixel_x;
    float   mickeysPerPixel_y;
    float   pixelPerMickey_x;
    float   pixelPerMickey_y;
    uint16_t  senv_x_val;
    uint16_t  senv_y_val;
    uint16_t  dspeed_val;
    float   senv_x;
    float   senv_y;
    int16_t  updateRegion_x[2];
    int16_t  updateRegion_y[2];
    uint16_t  doubleSpeedThreshold;
    uint16_t  language;
    uint16_t  cursorType;
    uint16_t  oldhidden;
    uint8_t  page;
    bool enabled;
    bool inhibit_draw;
    bool timer_in_progress;
    bool first_range_setx;
    bool first_range_sety;
    bool in_UIR;
    uint8_t mode;
    int16_t gran_x,gran_y;
    int scrollwheel;
    uint8_t ps2_type;
    uint8_t ps2_rate; // sampling rate is not really emulated, but needed for switching between protocols
    uint8_t ps2_packet_size;
    uint8_t ps2_unlock_idx;
} mouse;

double clamp(double d, double min, double max) {
  const double t = d < min ? min : d;
  return t > max ? max : t;
}

inline uint8_t GetWheel8bit() {
	/* CuteMouse wheel extension allows for -0x80,0x7F range, but let's keep it symmetric */
	int8_t tmp=clamp(mouse.wheel, static_cast<int16_t>(-0x7F), static_cast<int16_t>(0x7F));
	mouse.wheel=0;
	return (tmp >= 0) ? tmp : 0x100 + tmp;
}

inline uint8_t GetWheel16bit() {
	int16_t tmp=(mouse.wheel >= 0) ? mouse.wheel : 0x10000 + mouse.wheel;
	mouse.wheel=0;
	return tmp;
}

bool Mouse_SetPS2State(bool use) {
    if (use && (!ps2callbackinit)) {
        useps2callback = false;

        if (MOUSE_IRQ != 0)
            PIC_SetIRQMask(MOUSE_IRQ,true);

        return false;
    }
    useps2callback = use;
    Mouse_AutoLock(useps2callback);

    if (MOUSE_IRQ != 0)
        PIC_SetIRQMask(MOUSE_IRQ,!useps2callback);

    return true;
}

void Mouse_PS2Reset(void) {
	mouse.ps2_type       = TYPE_STANDARD;
	mouse.ps2_rate       = RATE_100;
	mouse.ps2_unlock_idx = 0;
}

bool Mouse_PS2SetPacketSize(uint8_t packet_size) {
	if ((packet_size==0x03) ||
		(packet_size==0x04 && mouse.ps2_type==TYPE_INTELLIMOUSE)) {
		mouse.ps2_packet_size = packet_size;
	    return true;
	}
	return false;
}

void Mouse_PS2SetSamplingRate(uint8_t rate) {
	mouse.ps2_rate = rate;
	if (UNLOCK_SEQ_INTELLIMOUSE[mouse.ps2_unlock_idx]!=rate)
		mouse.ps2_unlock_idx = 0;
	else if (UNLOCK_SEQ_INTELLIMOUSE.size()==++mouse.ps2_unlock_idx) {
			mouse.ps2_type=TYPE_INTELLIMOUSE;
			mouse.ps2_unlock_idx=0;
	}
}

uint8_t Mouse_PS2GetType(void) {
	return mouse.ps2_type;
}

void Mouse_ChangePS2Callback(uint16_t pseg, uint16_t pofs) {
    if ((pseg==0) && (pofs==0)) {
        ps2callbackinit = false;
        Mouse_AutoLock(false);
    } else {
        ps2callbackinit = true;
        ps2cbseg = pseg;
        ps2cbofs = pofs;
    }
    Mouse_AutoLock(ps2callbackinit);
}

/* set to true in case of shitty INT 15h device callbacks that fail to preserve CPU registers */
bool ps2_callback_save_regs = false;

void DoPS2Callback(uint16_t data, int16_t mouseX, int16_t mouseY) {
    if (useps2callback && ps2cbseg != 0 && ps2cbofs != 0) {
        uint16_t mdat = (data & 0x03) | 0x08;
        int16_t xdiff = mouseX-oldmouseX;
        int16_t ydiff = oldmouseY-mouseY;
        oldmouseX = mouseX;
        oldmouseY = mouseY;
        if ((xdiff>0xff) || (xdiff<-0xff)) mdat |= 0x40;        // x overflow
        if ((ydiff>0xff) || (ydiff<-0xff)) mdat |= 0x80;        // y overflow
        xdiff %= 256;
        ydiff %= 256;
        if (xdiff<0) {
            xdiff = (0x100+xdiff);
            mdat |= 0x10;
        }
        if (ydiff<0) {
            ydiff = (0x100+ydiff);
            mdat |= 0x20;
        }
        if (ps2_callback_save_regs) {
            CPU_Push16(reg_ax);CPU_Push16(reg_cx);CPU_Push16(reg_dx);CPU_Push16(reg_bx);
            CPU_Push16(reg_bp);CPU_Push16(reg_si);CPU_Push16(reg_di);
            CPU_Push16(SegValue(ds)); CPU_Push16(SegValue(es));
        }
        switch (mouse.ps2_packet_size) {
            case 0x04: // IntelliMouse protocol
                CPU_Push16((uint16_t)(mdat + (xdiff % 256) * 256));
                CPU_Push16((uint16_t)(ydiff % 256));
                CPU_Push16((uint16_t)GetWheel8bit());
                CPU_Push16((uint16_t)0);
                break;
            default:   // Standard protocol
                CPU_Push16((uint16_t)mdat);
                CPU_Push16((uint16_t)(xdiff % 256));
                CPU_Push16((uint16_t)(ydiff % 256));
                CPU_Push16((uint16_t)0);
        }
        CPU_Push16(RealSeg(ps2_callback));
        CPU_Push16(RealOff(ps2_callback));
        SegSet16(cs, ps2cbseg);
        reg_ip = ps2cbofs;
    }
}

Bitu PS2_Handler(void) {
    CPU_Pop16();CPU_Pop16();CPU_Pop16();CPU_Pop16();// remove the 4 words
    if (ps2_callback_save_regs) {
        SegSet16(es,CPU_Pop16()); SegSet16(ds,CPU_Pop16());
        reg_di=CPU_Pop16();reg_si=CPU_Pop16();reg_bp=CPU_Pop16();
        reg_bx=CPU_Pop16();reg_dx=CPU_Pop16();reg_cx=CPU_Pop16();reg_ax=CPU_Pop16();
    }
    return CBRET_NONE;
}


#define X_MICKEY 8
#define Y_MICKEY 8

#define MOUSE_HAS_MOVED 1
#define MOUSE_LEFT_PRESSED 2
#define MOUSE_LEFT_RELEASED 4
#define MOUSE_RIGHT_PRESSED 8
#define MOUSE_RIGHT_RELEASED 16
#define MOUSE_MIDDLE_PRESSED 32
#define MOUSE_MIDDLE_RELEASED 64
#define MOUSE_WHEEL_MOVED 128
#define MOUSE_ABSOLUTE 256
#define MOUSE_DUMMY 256

unsigned int user_mouse_report_rate = 0;
unsigned int mouse_report_rate = 200; /* DOSBox SVN compatible default (MOUSE_DELAY = 5.0 ms) */
pic_tickindex_t MOUSE_DELAY = 5.0; /* This was once a hard #define */

void UpdateMouseReportRate(void) {
	/* If the user did not specify an explicit rate, then whatever the current rate goes and update MOUSE_DELAY.
	 * This is done so that PS/2 mouse emulation can accept the commands to change reporting rate and have it work */
	if (user_mouse_report_rate != 0)
		mouse_report_rate = user_mouse_report_rate;

	MOUSE_DELAY = 1000.0 / mouse_report_rate;
}

void ChangeMouseReportRate(unsigned int new_rate) {
	if (user_mouse_report_rate == 0) {
		mouse_report_rate = new_rate;
		UpdateMouseReportRate();
	}
}

void MOUSE_Limit_Events(Bitu /*val*/) {
    mouse.timer_in_progress = false;

    if (IS_PC98_ARCH) {
        if (mouse.events>0) {
            mouse.events--;
        }
    }

    if (mouse.events) {
        mouse.timer_in_progress = true;
        PIC_AddEvent(MOUSE_Limit_Events,MOUSE_DELAY);

        if (MOUSE_IRQ != 0) {
            if (!IS_PC98_ARCH)
                PIC_ActivateIRQ(MOUSE_IRQ);
        }
    }
}

INLINE void Mouse_AddEvent(uint8_t type) {
    if (mouse.events<QUEUE_SIZE) {
        if (mouse.events>0) {
            /* Skip duplicate events */
            if (type==MOUSE_HAS_MOVED || type==MOUSE_WHEEL_MOVED) return;
            /* Always put the newest element in the front as that the events are 
             * handled backwards (prevents doubleclicks while moving)
             */
            for(Bitu i = mouse.events ; i ; i--)
                mouse.event_queue[i] = mouse.event_queue[i-1];
        }
        mouse.event_queue[0].type=type;
        mouse.event_queue[0].buttons=mouse.buttons;
        mouse.events++;
    }
    if (!mouse.timer_in_progress) {
        mouse.timer_in_progress = true;
        PIC_AddEvent(MOUSE_Limit_Events,MOUSE_DELAY);

        if (MOUSE_IRQ != 0) {
            if (!IS_PC98_ARCH)
                PIC_ActivateIRQ(MOUSE_IRQ);
        }
    }
}

void MOUSE_DummyEvent(void) {
    Mouse_AddEvent(MOUSE_DUMMY);
}

// ***************************************************************************
// Mouse cursor - text mode
// ***************************************************************************
/* Write and read directly to the screen. Do no use int_setcursorpos (LOTUS123) */
extern void WriteChar(uint16_t col,uint16_t row,uint8_t page,uint16_t chr,uint8_t attr,bool useattr);
extern void ReadCharAttr(uint16_t col,uint16_t row,uint8_t page,uint16_t * result);

void RestoreCursorBackgroundText() {
    if (mouse.hidden || mouse.inhibit_draw) return;

    if (mouse.background) {
        WriteChar((uint16_t)mouse.backposx,(uint16_t)mouse.backposy,real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE),mouse.backData[0],mouse.backData[1],true);
        mouse.background = false;
    }
}

void DrawCursorText() { 
    // Restore Background
    RestoreCursorBackgroundText();

    // Check if cursor in update region
    if ((POS_Y <= mouse.updateRegion_y[1]) && (POS_Y >= mouse.updateRegion_y[0]) &&
        (POS_X <= mouse.updateRegion_x[1]) && (POS_X >= mouse.updateRegion_x[0])) {
        return;
    }

    // Save Background
    mouse.backposx      = POS_X>>3;
    mouse.backposy      = POS_Y>>3;
    if (mouse.mode < 2) mouse.backposx >>= 1; 

    //use current page (CV program)
    uint8_t page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);

    if (mouse.cursorType == 0) {
        uint16_t result;
        ReadCharAttr((uint16_t)mouse.backposx,(uint16_t)mouse.backposy,page,&result);
        mouse.backData[0]	= (uint8_t)(result & 0xFF);
        mouse.backData[1]	= (uint8_t)(result>>8);
        mouse.background	= true;
        // Write Cursor
        result = (result & mouse.textAndMask) ^ mouse.textXorMask;
        WriteChar((uint16_t)mouse.backposx,(uint16_t)mouse.backposy,page,(uint8_t)(result&0xFF),(uint8_t)(result>>8),true);
    } else {
        uint16_t address=page * real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE);
        address += (mouse.backposy * real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS) + mouse.backposx) * 2;
        address /= 2;
        uint16_t cr = real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
        IO_Write(cr     , 0xe);
        IO_Write((Bitu)cr + 1u, (address >> 8) & 0xff);
        IO_Write(cr     , 0xf);
        IO_Write((Bitu)cr + 1u, address & 0xff);
    }
}

// ***************************************************************************
// Mouse cursor - graphic mode
// ***************************************************************************

static uint8_t gfxReg3CE[9];
static uint8_t index3C4,gfxReg3C5;
void SaveVgaRegisters() {
    if (IS_VGA_ARCH) {
        for (uint8_t i=0; i<9; i++) {
            IO_Write    (0x3CE,i);
            gfxReg3CE[i] = IO_Read(0x3CF);
        }
        /* Setup some default values in GFX regs that should work */
        IO_Write(0x3CE,3); IO_Write(0x3Cf,0);               //disable rotate and operation
        IO_Write(0x3CE,5); IO_Write(0x3Cf,gfxReg3CE[5]&0xf0);   //Force read/write mode 0

        //Set Map to all planes. Celtic Tales
        index3C4 = IO_Read(0x3c4);  IO_Write(0x3C4,2);
        gfxReg3C5 = IO_Read(0x3C5); IO_Write(0x3C5,0xF);
    } else if (machine==MCH_EGA) {
        //Set Map to all planes.
        IO_Write(0x3C4,2);
        IO_Write(0x3C5,0xF);
    }
}

void RestoreVgaRegisters() {
    if (IS_VGA_ARCH) {
        for (uint8_t i=0; i<9; i++) {
            IO_Write(0x3CE,i);
            IO_Write(0x3CF,gfxReg3CE[i]);
        }

        IO_Write(0x3C4,2);
        IO_Write(0x3C5,gfxReg3C5);
        IO_Write(0x3C4,index3C4);
    }
}

void ClipCursorArea(int16_t& x1, int16_t& x2, int16_t& y1, int16_t& y2,
                    uint16_t& addx1, uint16_t& addx2, uint16_t& addy) {
    addx1 = addx2 = addy = 0;
    // Clip up
    if (y1<0) {
        addy += (-y1);
        y1 = 0;
    }
    // Clip down
    if (y2>mouse.clipy) {
        y2 = mouse.clipy;       
    }
    // Clip left
    if (x1<0) {
        addx1 += (-x1);
        x1 = 0;
    }
    // Clip right
    if (x2>mouse.clipx) {
        addx2 = x2 - mouse.clipx;
        x2 = mouse.clipx;
    }
}

void RestoreCursorBackground() {
    if (mouse.hidden || mouse.inhibit_draw) return;

    SaveVgaRegisters();
    if (mouse.background) {
        // Restore background
        int16_t x,y;
        uint16_t addx1,addx2,addy;
        uint16_t dataPos  = 0;
        int16_t x1       = mouse.backposx;
        int16_t y1       = mouse.backposy;
        int16_t x2       = x1 + CURSORX - 1;
        int16_t y2       = y1 + CURSORY - 1; 

        ClipCursorArea(x1, x2, y1, y2, addx1, addx2, addy);

        dataPos = addy * CURSORX;
        for (y=y1; y<=y2; y++) {
            dataPos += addx1;
            for (x=x1; x<=x2; x++) {
                INT10_PutPixel((uint16_t)x,(uint16_t)y,mouse.page,mouse.backData[dataPos++]);
            }
            dataPos += addx2;
        }
        mouse.background = false;
    }
    RestoreVgaRegisters();
}

void DrawCursor() {
    if (mouse.hidden || mouse.inhibit_draw) return;
    if (usesystemcursor&&!MOUSE_IsLocked()) {
        SDL_ShowCursor(SDL_ENABLE);
        return;
    }
    INT10_SetCurMode();
    // In Textmode ?
    if (CurMode->type==M_TEXT) {
        DrawCursorText();
        return;
    }

    // Check video page. Seems to be ignored for text mode. 
    // hence the text mode handled above this
    // >>> removed because BIOS page is not actual page in some cases, e.g. QQP games
    // if (real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE)!=mouse.page) return;

// Check if cursor in update region
/*  if ((POS_X >= mouse.updateRegion_x[0]) && (POS_X <= mouse.updateRegion_x[1]) &&
        (POS_Y >= mouse.updateRegion_y[0]) && (POS_Y <= mouse.updateRegion_y[1])) {
        if (CurMode->type==M_TEXT16)
            RestoreCursorBackgroundText();
        else
            RestoreCursorBackground();
        mouse.shown--;
        return;
    }
   */ /*Not sure yet what to do update region should be set to ??? */
         
    // Get Clipping ranges


    mouse.clipx = (int16_t)((Bits)CurMode->swidth-1);    /* Get from bios ? */
    mouse.clipy = (int16_t)((Bits)CurMode->sheight-1);

    /* might be vidmode == 0x13?2:1 */
    int16_t xratio = 640;
    if (CurMode->swidth>0) xratio/=(uint16_t)CurMode->swidth;
    if (xratio==0) xratio = 1;
    
    RestoreCursorBackground();

    SaveVgaRegisters();

    // Save Background
    int16_t x,y;
    uint16_t addx1,addx2,addy;
    uint16_t dataPos  = 0;
    int16_t x1       = POS_X / xratio - mouse.hotx;
    int16_t y1       = POS_Y - mouse.hoty;
    int16_t x2       = x1 + CURSORX - 1;
    int16_t y2       = y1 + CURSORY - 1; 

    ClipCursorArea(x1,x2,y1,y2, addx1, addx2, addy);

    dataPos = addy * CURSORX;
    for (y=y1; y<=y2; y++) {
        dataPos += addx1;
        for (x=x1; x<=x2; x++) {
            INT10_GetPixel((uint16_t)x,(uint16_t)y,mouse.page,&mouse.backData[dataPos++]);
        }
        dataPos += addx2;
    }
    mouse.background= true;
    mouse.backposx  = POS_X / xratio - mouse.hotx;
    mouse.backposy  = POS_Y - mouse.hoty;

    // Draw Mousecursor
    dataPos = addy * CURSORX;
    for (y=y1; y<=y2; y++) {
        uint16_t scMask = mouse.screenMask[addy+y-y1];
        uint16_t cuMask = mouse.cursorMask[addy+y-y1];
        if (addx1>0) { scMask<<=addx1; cuMask<<=addx1; dataPos += addx1; }
        for (x=x1; x<=x2; x++) {
            uint8_t pixel = 0;
            // ScreenMask
            if (scMask & HIGHESTBIT) pixel = mouse.backData[dataPos];
            scMask<<=1;
            // CursorMask
            if (cuMask & HIGHESTBIT) pixel = pixel ^ 0x0F;
            cuMask<<=1;
            // Set Pixel
            INT10_PutPixel((uint16_t)x,(uint16_t)y,mouse.page,pixel);
            dataPos++;
        }
        dataPos += addx2;
    }
    RestoreVgaRegisters();
}

void pc98_mouse_movement_apply(int x,int y);

#if !defined(C_SDL2)
bool GFX_IsFullscreen(void);
#else
static inline bool GFX_IsFullscreen(void) {
    return false;
}
#endif
#include "render.h"

void KEYBOARD_AUX_LowerIRQ() {
    if (MOUSE_IRQ != 0)
        PIC_SetIRQMask(MOUSE_IRQ,false);
}

extern int  user_cursor_x,  user_cursor_y;
extern int  user_cursor_sw, user_cursor_sh;
extern bool user_cursor_locked;

/* Do not access BIOS data areas or other real-mode areas
 * unless running in real mode or virtual 8086 mode, AND the
 * DOS kernel must not be shutdown since INT 33h is an MS-DOS
 * interface. That means booting into a guest OS should disable
 * all INT 33h emulation leaving only PS/2, serial, and AUX. */
static bool AllowINT33RMAccess() {
	if (!dos_kernel_disabled) {
		if (!cpu.pmode) // not protected mode
			return true;
		if (GETFLAG(VM)) // protected mode, but virtual 8086 mode
			return true;
	}

	return false;
}

/* FIXME: Re-test this code */
void Mouse_CursorMoved(float xrel,float yrel,float x,float y,bool emulate) {
    extern bool Mouse_Vertical;
    float dx = xrel * mouse.pixelPerMickey_x;
    float dy = (Mouse_Vertical?-yrel:yrel) * mouse.pixelPerMickey_y;

    if (!IS_PC98_ARCH && KEYBOARD_AUX_Active()) {
        KEYBOARD_AUX_Event(xrel,yrel,mouse.buttons,mouse.scrollwheel);
        mouse.scrollwheel = 0;
        return;
    }

    if((fabs(xrel) > 1.0) || (mouse.senv_x < 1.0)) dx *= mouse.senv_x;
    if((fabs(yrel) > 1.0) || (mouse.senv_y < 1.0)) dy *= mouse.senv_y;
    if (useps2callback) dy *= 2;    

    if (user_cursor_locked) {
        /* either device reports relative motion ONLY, and therefore requires that the user
         * has captured the mouse */

        /* serial mouse */
        on_mouse_event_for_serial((int)(dx),(int)(dy*2),mouse.buttons);

        /* PC-98 mouse */
        if (IS_PC98_ARCH) pc98_mouse_movement_apply(xrel,yrel);

        mouse.mickey_x += (dx * mouse.mickeysPerPixel_x);
        mouse.mickey_y += (dy * mouse.mickeysPerPixel_y);
        if (mouse.mickey_x >= 32768.0) mouse.mickey_x -= 65536.0;
        else if (mouse.mickey_x <= -32769.0) mouse.mickey_x += 65536.0;
        if (mouse.mickey_y >= 32768.0) mouse.mickey_y -= 65536.0;
        else if (mouse.mickey_y <= -32769.0) mouse.mickey_y += 65536.0;
    }

    if (emulate) {
        mouse.x += dx;
        mouse.y += dy;
    } else if (AllowINT33RMAccess() && CurMode != NULL) {
        if (CurMode->type == M_TEXT) {
            mouse.x = x*real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8;
            mouse.y = y*(IS_EGAVGA_ARCH?(real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1):25)*8;
        /* NTS: DeluxePaint II enhanced sets a large range (5112x3832) for VGA mode 0x12 640x480 16-color */
        } else {
            if ((mouse.max_x > 0) && (mouse.max_y > 0)) {
                mouse.x = x*mouse.max_x;
                mouse.y = y*mouse.max_y;
            } else {
                mouse.x += xrel;
                mouse.y += yrel;
            }
        }
    }

    /* ignore constraints if using PS2 mouse callback in the bios */

    if (mouse.x > mouse.max_x) mouse.x = mouse.max_x;
    if (mouse.x < mouse.min_x) mouse.x = mouse.min_x;
    if (mouse.y > mouse.max_y) mouse.y = mouse.max_y;
    if (mouse.y < mouse.min_y) mouse.y = mouse.min_y;

    /*make mouse emulated, eventually*/
    extern MOUSE_EMULATION user_cursor_emulation;
    bool emu;
    switch (user_cursor_emulation)
    {
    case MOUSE_EMULATION_ALWAYS:
        emu = true;
        break;
    case MOUSE_EMULATION_INTEGRATION:
        emu = !user_cursor_locked && !GFX_IsFullscreen();
        break;
    case MOUSE_EMULATION_LOCKED:
        emu = user_cursor_locked && !GFX_IsFullscreen();
        break;
    case MOUSE_EMULATION_NEVER:
    default:
        emu = false;
    }
    if (!emu)
    {
        auto x1 = (double)user_cursor_x / ((double)user_cursor_sw - 1);
        auto y1 = (double)user_cursor_y / ((double)user_cursor_sh - 1);
        mouse.x       = x1 * mouse.max_screen_x;
        mouse.y       = y1 * mouse.max_screen_y;

        if (mouse.x < mouse.min_x)
            mouse.x = mouse.min_x;
        if (mouse.y < mouse.min_y)
            mouse.y = mouse.min_y;
        if (mouse.x > mouse.max_x)
            mouse.x = mouse.max_x;
        if (mouse.y > mouse.max_y)
            mouse.y = mouse.max_y;
    }

    if (user_cursor_locked) {
        /* send relative PS/2 mouse motion only if the cursor is captured */
        mouse.ps2x += xrel;
        mouse.ps2y += yrel;
        if (mouse.ps2x >= 32768.0)       mouse.ps2x -= 65536.0;
        else if (mouse.ps2x <= -32769.0) mouse.ps2x += 65536.0;
        if (mouse.ps2y >= 32768.0)       mouse.ps2y -= 65536.0;
        else if (mouse.ps2y <= -32769.0) mouse.ps2y += 65536.0;
    }

    Mouse_AddEvent(MOUSE_HAS_MOVED);
}

uint8_t Mouse_GetButtonState(void) {
    return mouse.buttons;
}

char text[5000];
extern std::list<uint16_t> bdlist;
extern bool isDBCSCP();
extern std::vector<std::pair<int,int>> jtbs, dbox;
extern std::map<int, int> pc98boxdrawmap;
const char* Mouse_GetSelected(int x1, int y1, int x2, int y2, int w, int h, uint16_t *textlen) {
    bdlist = {};
	uint16_t result=0, len=0;
	uint8_t page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
	uint16_t c=0, r=0;
	if (IS_PC98_ARCH) {
		c=80;
		r=real_readb(0x60,0x113) & 0x01 ? 25 : 20;
	} else {
		c=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
		r=(uint16_t)(IS_EGAVGA_ARCH?real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS):24)+1;
	}
    int c1=x1, r1=y1, c2=x2, r2=y2, t;
    if (w>0&&h>0) {
        c1=c*x1/w;
        r1=r*y1/h;
        c2=c*x2/w;
        r2=r*y2/h;
    }
	if (c1>c2) {
		t=c1;
		c1=c2;
		c2=t;
	}
	if (r1>r2) {
		t=r1;
		r1=r2;
		r2=t;
	}
	text[0]=0;
    uint16_t seg = IS_DOSV?GetTextSeg():VGAMEM_CTEXT;
    bool ttfuse = false;
    int ttfcols = 0;
#if defined(USE_TTF)
    ttfuse = ttf.inUse;
    ttfcols = ttf.cols;
    if (ttfuse&&isDBCSCP()&&dbcs_sbcs&&!(c1==0&&c2==(int)(ttf.cols-1)&&r1==0&&r2==(int)(ttf.lins-1))) {
        ttf_cell *curAC = curAttrChar;
        for (unsigned int y = 0; y < ttf.lins; y++) {
            if ((int)y>=r1&&(int)y<=r2) {
                for (unsigned int x = 0; x < ttf.cols; x++)
                    if ((int)x>=c1&&(int)x<=c2&&curAC[rtl?ttf.cols-x-1:x].selected) {
                        if ((int)x==c1&&c1>0&&curAC[rtl?ttf.cols-x-1:x].skipped&&!curAC[rtl?ttf.cols-x-2:x-1].selected&&curAC[rtl?ttf.cols-x-2:x-1].doublewide) {
                            ReadCharAttr(rtl?ttf.cols-x-2:x-1,y,page,&result);
                            text[len++]=result;
                        }
                        ReadCharAttr(rtl?ttf.cols-x-1:x,y,page,&result);
                        if (curAC[rtl?ttf.cols-x-1:x].boxdraw||(!x&&curAC[rtl?ttf.cols-x:x+1].boxdraw)) bdlist.push_back(len);
                        text[len++]=result;
                        if ((int)x==c2&&c2<(int)(ttf.cols-1)&&curAC[rtl?ttf.cols-x-1:x].doublewide&&!curAC[rtl?ttf.cols-x:x+1].selected&&curAC[rtl?ttf.cols-x:x+1].skipped) {
                            ReadCharAttr(rtl?ttf.cols-x:x+1,y,page,&result);
                            text[len++]=result;
                        }
                    }
                while (len>0&&text[len-1]==32) text[--len]=0;
                if (y<r2) text[len++]='\n';
            }
            curAC += ttf.cols;
        }
    } else
#endif
	for (int i=r1; i<=r2; i++) {
        uint16_t startlen = len;
        bool lead1 = false, lead2 = false;
        if ((showdbcs && !ttfuse && !IS_DOSV && isDBCSCP()
#if defined(USE_TTF)
            && dbcs_sbcs
#endif
            ) || (IS_DOSV && DOSV_CheckCJKVideoMode()) || (IS_J3100 && J3_IsJapanese())) {
            for (int k=0; k<c1; k++) {
                if (lead1) lead1=false;
                else if (isKanji1(real_readb(seg,(i*c+k)*2))) lead1=true;
            }
        }
		for (int j=c1+(lead1?1:0); j<=c2; j++) {
			if (IS_PC98_ARCH) {
				uint16_t address=((i*80)+(ttfuse&&rtl?ttfcols-j-1:j))*2;
				PhysPt where = CurMode->pstart+address;
				result=mem_readw(where);
                if (!result || (result == 0xFE && j && mem_readw(where-2) == 0)) result = 32;
                if ((result & 0xFF00u) != 0u && (result & 0x7Cu) == 0x08u && (result%0xff) - (result/0x100) == 0xB) {
                    uint8_t val = result/0x100+31;
                    for (auto it = pc98boxdrawmap.begin(); it != pc98boxdrawmap.end(); ++it)
                        if (it->second == val) {
                            text[len++]=it->first;
                            break;
                        }
                } else if ((result & 0xFF00u) != 0u && (result & 0x7Cu) != 0x08u && result==mem_readw(where+(ttfuse&&rtl?-2:2)) && ++j<c) {
					result&=0x7F7F;
					uint8_t j1=(result%0x100)+0x20, j2=result/0x100;
					if (j1>32&&j1<127&&j2>32&&j2<127) {
						if (ttfuse&&rtl) text[len++]=j2+(j1%2?31+(j2/96):126);
						text[len++]=(j1+1)/2+(j1<95?112:176);
						if (!ttfuse||!rtl) text[len++]=j2+(j1%2?31+(j2/96):126);
						if (del_flag && (text[len-1]&0xFF) == 0x7F) text[len-1]++;
					}
				} else if (j==c1&&c1>0) {
                    uint16_t prevres=mem_readw(where-(ttfuse&&rtl?-2:2));
                    if (!((prevres & 0xFF00u) != 0u && (prevres & 0xFCu) != 0x08u && prevres==result))
                        text[len++]=result;
                } else if (result)
                    text[len++]=result;
            } else if ((IS_DOSV && DOSV_CheckCJKVideoMode()) || (IS_J3100 && J3_IsJapanese())) {
                if (lead2) lead2=false;
                else if (isKanji1(real_readb(seg,(i*c+j)*2))) lead2=true;
                result=real_readb(seg,(i*c+j)*2);
                if ((uint8_t)result==0) result=32;
                text[len++]=result;
                if (!lead2 && del_flag && (text[len-1]&0xFF) == 0x7F) text[len-1]++;
                if (j==c2&&c2<c-1&&lead2) {
                    result=real_readb(seg,(i*c+j+1)*2);
                    text[len++]=result;
                    if ((IS_JDOSV || dos.loaded_codepage == 932) && del_flag && (text[len-1]&0xFF) == 0x7F) text[len-1]++;
                }
            } else {
                bool find = isJEGAEnabled() || (isDBCSCP()
#if defined(USE_TTF)
                && dbcs_sbcs
#endif
                && showdbcs) ? std::find(jtbs.begin(), jtbs.end(), std::make_pair(i,j)) != jtbs.end():false;
                if (!isJEGAEnabled()||j>c1||!find) {
                    ReadCharAttr(ttfuse&&rtl?ttfcols-j-1:j,i,page,&result);
                    if ((uint8_t)result==0) result=32;
#if defined(USE_TTF)
                    if (ttfuse && isDBCSCP()) {
                        ttf_cell *curAC = curAttrChar+i*ttfcols;
                        if (curAC[rtl?ttfcols-j-1:j].boxdraw||(!j&&curAC[rtl?ttf.cols-j:j+1].boxdraw)) bdlist.push_back(len);
                    } else
#endif
                    if (isDBCSCP()
#if defined(USE_TTF)
                    && dbcs_sbcs
#endif
                    && showdbcs && std::find(dbox.begin(), dbox.end(), std::make_pair(i,j)) != dbox.end()) bdlist.push_back(len);
                    text[len++]=result;
                    if (dos.loaded_codepage == 932 && find && del_flag && (text[len-1]&0xFF) == 0x7F) text[len-1]++;
                }
                if ((isJEGAEnabled()||(isDBCSCP()
#if defined(USE_TTF)
                && dbcs_sbcs
#endif
                && showdbcs))&&j==c2&&c2<c-1&&std::find(jtbs.begin(), jtbs.end(), std::make_pair(i,j+1)) != jtbs.end()) {
                    ReadCharAttr(ttfuse&&rtl?(ttfcols-j):(j+1),i,page,&result);
                    text[len++]=result;
                    if (dos.loaded_codepage == 932 && del_flag && (text[len-1]&0xFF) == 0x7F) text[len-1]++;
                }
			}
		}
		if (ttfuse&&rtl) std::reverse(text+startlen, text+len);
		while (len>0&&text[len-1]==32) {text[--len]=0;bdlist.remove(len);}
		if (i<r2) text[len++]='\n';
	}
    text[len] = 0;
	*textlen=len;
	return text;
}

#if defined(WIN32) || defined(MACOSX) || defined(C_SDL2)
void Mouse_Select(int x1, int y1, int x2, int y2, int w, int h, bool select) {
    int c1=x1, r1=y1, c2=x2, r2=y2, t;
    uint8_t page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
    uint16_t c=0, r=0;
    if (IS_PC98_ARCH) {
        c=80;
        r=real_readb(0x60,0x113) & 0x01 ? 25 : 20;
    } else {
        c=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
        r=(uint16_t)(IS_EGAVGA_ARCH?real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS):24)+1;
    }
    if (w>0&&h>0) {
        c1=c*x1/w;
        r1=r*y1/h;
        c2=c*x2/w;
        r2=r*y2/h;
    }
	if (c1>c2) {
		t=c1;
		c1=c2;
		c2=t;
	}
	if (r1>r2) {
		t=r1;
		r1=r2;
		r2=t;
	}
    uint16_t seg = IS_DOSV?GetTextSeg():0;
    bool ttfuse = false;
    int ttfcols = 0;
#if defined(USE_TTF)
    ttfuse = ttf.inUse;
    ttfcols = ttf.cols;
    if (ttfuse&&(!IS_EGAVGA_ARCH||CurMode->mode!=3||(isDBCSCP()&&dbcs_sbcs))) {
        ttf_cell *newAC = newAttrChar;
        for (unsigned int y = 0; y < ttf.lins; y++) {
            if (y>=r1&&y<=r2)
                for (unsigned int x = 0; x < ttf.cols; x++)
                    if ((x>=c1||((IS_PC98_ARCH||(isDBCSCP()&&dbcs_sbcs))&&c1>0&&x==c1-1&&(newAC[rtl?ttf.cols-x-1:x].chr&0xFF00)&&(newAC[rtl?ttf.cols-x:x+1].chr&0xFF)==32))&&x<=c2)
                        newAC[rtl?ttf.cols-x-1:x].selected = select?1:0;
            newAC += ttf.cols;
        }
        void GFX_EndTextLines(bool force=false);
        GFX_EndTextLines();
    } else
#endif
	for (int i=r1; i<=r2; i++)
		for (int j=c1; j<=c2; j++) {
			if (IS_PC98_ARCH) {
				PhysPt where = CurMode->pstart+((i*80)+j)*2;
				mem_writeb(where+0x2000,mem_readb(where+0x2000)^16);
			} else if ((IS_DOSV && DOSV_CheckCJKVideoMode()) || (IS_J3100 && J3_IsJapanese())) {
				uint8_t attr = real_readb(seg,(i*c+j)*2+1);
				real_writeb(seg,(i*c+j)*2+1,attr/0x10+(attr&0xF)*0x10);
				if (j==c2) WriteCharTopView(c*i*2,j+1);
			} else if (CurMode->type != M_DCGA)
				real_writeb(0xb800,(i*c+(ttfuse&&rtl?ttfcols-j-1:j))*2+1,real_readb(0xb800,(i*c+(ttfuse&&rtl?ttfcols-j-1:j))*2+1)^119);
		}
}
#endif

void Mouse_ButtonPressed(uint8_t button) {
    if (!IS_PC98_ARCH && KEYBOARD_AUX_Active()) {
        switch (button) {
            case 0:
                mouse.buttons|=1;
                break;
            case 1:
                mouse.buttons|=2;
                break;
            case 2:
                mouse.buttons|=4;
                break;
            default:
                return;
        }

        KEYBOARD_AUX_Event(0,0,mouse.buttons,mouse.scrollwheel);
        mouse.scrollwheel = 0;
        return;
    }

    if (button > 2)
        return;

    switch (button) {
#if (MOUSE_BUTTONS >= 1)
    case 0:
        if (mouse.buttons&1) return;
        mouse.buttons|=1;
        Mouse_AddEvent(MOUSE_LEFT_PRESSED);
        break;
#endif
#if (MOUSE_BUTTONS >= 2)
    case 1:
        if (mouse.buttons&2) return;
        mouse.buttons|=2;
        Mouse_AddEvent(MOUSE_RIGHT_PRESSED);
        break;
#endif
#if (MOUSE_BUTTONS >= 3)
    case 2:
        if (mouse.buttons&4) return;
        mouse.buttons|=4;
        Mouse_AddEvent(MOUSE_MIDDLE_PRESSED);
        break;
#endif
    default:
        return;
    }
    mouse.times_pressed[button]++;
    mouse.last_pressed_x[button]=(uint16_t)POS_X;
    mouse.last_pressed_y[button]=(uint16_t)POS_Y;

    /* serial mouse, if connected, also wants to know about it */
    on_mouse_event_for_serial(0,0,mouse.buttons);
}

void Mouse_ButtonReleased(uint8_t button) {
    if (!IS_PC98_ARCH && KEYBOARD_AUX_Active()) {
        switch (button) {
            case 0:
                mouse.buttons&=~1;
                break;
            case 1:
                mouse.buttons&=~2;
                break;
            case 2:
                mouse.buttons&=~4;
                break;
            case (100-1):   /* scrollwheel up */
                mouse.scrollwheel -= 8;
                break;
            case (100+1):   /* scrollwheel down */
                mouse.scrollwheel += 8;
                break;
            default:
                return;
        }

        KEYBOARD_AUX_Event(0,0,mouse.buttons,mouse.scrollwheel);
        mouse.scrollwheel = 0;
        return;
    }

    if (button > 2)
        return;

    switch (button) {
#if (MOUSE_BUTTONS >= 1)
    case 0:
        if (!(mouse.buttons&1)) return;
        mouse.buttons&=~1;
        Mouse_AddEvent(MOUSE_LEFT_RELEASED);
        break;
#endif
#if (MOUSE_BUTTONS >= 2)
    case 1:
        if (!(mouse.buttons&2)) return;
        mouse.buttons&=~2;
        Mouse_AddEvent(MOUSE_RIGHT_RELEASED);
        break;
#endif
#if (MOUSE_BUTTONS >= 3)
    case 2:
        if (!(mouse.buttons&4)) return;
        mouse.buttons&=~4;
        Mouse_AddEvent(MOUSE_MIDDLE_RELEASED);
        break;
#endif
    default:
        return;
    }
    mouse.times_released[button]++; 
    mouse.last_released_x[button]=(uint16_t)POS_X;
    mouse.last_released_y[button]=(uint16_t)POS_Y;

    /* serial mouse, if connected, also wants to know about it */
    on_mouse_event_for_serial(0,0,mouse.buttons);
}

void Mouse_WheelMoved(int32_t scroll) {
	mouse.wheel = clamp(scroll + mouse.wheel, -0x7FFF, 0x7FFF); /* limit is -0x8000,0x7FFF, but let's keep it symmetric */
	Mouse_AddEvent(MOUSE_WHEEL_MOVED);
	mouse.last_scrolled_x=POS_X;
	mouse.last_scrolled_y=POS_Y;
}

static void Mouse_SetMickeyPixelRate(int16_t px, int16_t py){
    if ((px!=0) && (py!=0)) {
        mouse.mickeysPerPixel_x  = (float)px/X_MICKEY;
        mouse.mickeysPerPixel_y  = (float)py/Y_MICKEY;
        mouse.pixelPerMickey_x   = X_MICKEY/(float)px;
        mouse.pixelPerMickey_y   = Y_MICKEY/(float)py;  
    }
}

static void Mouse_SetSensitivity(uint16_t px, uint16_t py, uint16_t dspeed){
    if(px>100) px=100;
    if(py>100) py=100;
    if(dspeed>100) dspeed=100;
    // save values
    mouse.senv_x_val=px;
    mouse.senv_y_val=py;
    mouse.dspeed_val=dspeed;
    if ((px!=0) && (py!=0)) {
        px--;  //Inspired by cutemouse 
        py--;  //Although their cursor update routine is far more complex then ours
        mouse.senv_x=(static_cast<float>(px)*px)/3600.0f +1.0f/3.0f;
        mouse.senv_y=(static_cast<float>(py)*py)/3600.0f +1.0f/3.0f;
     }
}


static void Mouse_ResetHardware(void){
    if (MOUSE_IRQ != 0)
        PIC_SetIRQMask(MOUSE_IRQ,false);

    if (IS_PC98_ARCH) {
        IO_WriteB(0x7FDD,IO_ReadB(0x7FDD) & (~0x10)); // remove interrupt inhibit

        // NEC MOUSE.COM behavior: Driver startup and INT 33h AX=0 automatically show the graphics layer.
        // Some games by "Orange House" depend on this behavior, without which the graphics are invisible.
        if (en_int33_pc98_show_graphics) {
            reg_eax = 0x40u << 8u; // AH=40h show graphics layer
            CALLBACK_RunRealInt(0x18);
        }
    }
}

void Mouse_BeforeNewVideoMode(bool setmode) {
    (void)setmode;//unused

    if (CurMode->type!=M_TEXT) RestoreCursorBackground();
    else RestoreCursorBackgroundText();
    if (!mouse.hidden) mouse.hidden_at = PIC_FullIndex();
    mouse.hidden = 1;
    mouse.oldhidden = 1;
    mouse.background = false;
}

//Does way to much. Many things should be moved to mouse reset one day
void Mouse_AfterNewVideoMode(bool setmode) {
    mouse.inhibit_draw = false;
    /* Get the correct resolution from the current video mode */
    uint8_t mode = mem_readb(BIOS_VIDEO_MODE);
    if (setmode && mode == mouse.mode) LOG(LOG_MOUSE,LOG_NORMAL)("New video mode is the same as the old");
    mouse.first_range_setx = false;
    mouse.first_range_sety = false;
    mouse.gran_x = (int16_t)0xffff;
    mouse.gran_y = (int16_t)0xffff;

    /* If new video mode is SVGA and this is NOT a mouse driver reset, then do not reset min/max.
     * This is needed for "down-by-the-laituri-peli" (some sort of Finnish band tour simulation?)
     * which sets the min/max range and THEN sets up 640x480 256-color mode through the VESA BIOS.
     * Without this fix, the cursor is constrained to the upper left hand quadrant of the screen. */
    if (!setmode || mode <= 0x13/*non-SVGA*/) {
        mouse.min_x = 0;
        mouse.max_x = 639;
        mouse.min_y = 0;
        mouse.max_y = 479;
    }

    if (machine == MCH_HERC) {
        // DeluxePaint II again...
        mouse.first_range_setx = true;
        mouse.first_range_sety = true;
    }

    switch (mode) {
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x07: {
        mouse.gran_x = (mode<2)?0xfff0:0xfff8;
        mouse.gran_y = (int16_t)0xfff8;
        if (IS_PC98_ARCH) {
            mouse.max_y = 400 - 1;
        }
        else {
            Bitu rows = IS_EGAVGA_ARCH?real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS):24;
            if ((rows == 0) || (rows > 250)) rows = 25 - 1;
            mouse.max_y = 8*(rows+1) - 1;
        }
        break;
    }
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x08:
    case 0x09:
    case 0x0a:
    case 0x0d:
    case 0x0e:
    case 0x13:
        if (mode == 0x0d || mode == 0x13) mouse.gran_x = (int16_t)0xfffe;
        mouse.max_y = 199;
        // some games redefine the mouse range for this mode
        mouse.first_range_setx = true;
        mouse.first_range_sety = true;
        break;
    case 0x0f:
    case 0x10:
        mouse.max_y = 349;
        // Deluxepaint II enhanced will redefine the range for 640x480 mode
        mouse.first_range_setx = true;
        mouse.first_range_sety = true;
        break;
    case 0x11:
    case 0x12:
        mouse.max_y = 479;
        // Deluxepaint II enhanced will redefine the range for 640x480 mode
        mouse.first_range_setx = true;
        mouse.first_range_sety = true;
        break;
    default:
        LOG(LOG_MOUSE,LOG_ERROR)("Unhandled videomode %X on reset",mode);
        mouse.inhibit_draw = true;
        /* If new video mode is SVGA and this is NOT a mouse driver reset, then do not reset min/max.
         * This is needed for "down-by-the-laituri-peli" (some sort of Finnish band tour simulation?)
         * which sets the min/max range and THEN sets up 640x480 256-color mode through the VESA BIOS.
         * Without this fix, the cursor is constrained to the upper left hand quadrant of the screen. */
        if (!setmode) {
            if (CurMode != NULL) {
                mouse.first_range_setx = true;
                mouse.first_range_sety = true;
                mouse.max_x = CurMode->swidth - 1;
                mouse.max_y = CurMode->sheight - 1;
            }
            else {
                mouse.max_x = 639;
                mouse.max_y = 479;
            }
        }
        break;
    }
    mouse.mode = mode;

    if (cell_granularity_disable) {
        mouse.gran_x = (int16_t)0xffff;
        mouse.gran_y = (int16_t)0xffff;
    }

    mouse.events = 0;
    mouse.timer_in_progress = false;
    PIC_RemoveEvents(MOUSE_Limit_Events);

    mouse.hotx       = 0;
    mouse.hoty       = 0;
    mouse.background = false;
    mouse.screenMask = defaultScreenMask;
    mouse.cursorMask = defaultCursorMask;
    mouse.textAndMask= defaultTextAndMask;
    mouse.textXorMask= defaultTextXorMask;
    mouse.language   = 0;
    mouse.page               = 0;
    mouse.doubleSpeedThreshold = 64;
    mouse.updateRegion_y[1] = -1; //offscreen
    mouse.cursorType = 0;
    mouse.enabled=true;

    mouse.max_screen_x = mouse.max_x;
    mouse.max_screen_y = mouse.max_y;
}

//Much too empty, Mouse_NewVideoMode contains stuff that should be in here
static void Mouse_Reset(void) {
    Mouse_BeforeNewVideoMode(false);
    Mouse_AfterNewVideoMode(false);
    Mouse_SetMickeyPixelRate(8,16);

    mouse.mickey_x = 0;
    mouse.mickey_y = 0;

    mouse.buttons = 0;

    mouse.wheel           = 0; /* CuteMouse wheel extension */
    mouse.last_scrolled_x = 0;
    mouse.last_scrolled_y = 0;

    for (uint16_t but=0; but<MOUSE_BUTTONS; but++) {
        mouse.times_pressed[but] = 0;
        mouse.times_released[but] = 0;
        mouse.last_pressed_x[but] = 0;
        mouse.last_pressed_y[but] = 0;
        mouse.last_released_x[but] = 0;
        mouse.last_released_y[but] = 0;
    }

    // Dont set max coordinates here. it is done by SetResolution!
    mouse.x = static_cast<float>((mouse.max_x + 1)/ 2);
    mouse.y = static_cast<float>((mouse.max_y + 1)/ 2);
    mouse.sub_mask = 0;
    mouse.in_UIR = false;
}

void Mouse_Used(void) {
    static bool autolock_enabled = false;
    if(!autolock_enabled) {
        Mouse_AutoLock(true);
        autolock_enabled = true;
    }
}

void Mouse_Read_Motion_Data() {
    const auto locked = MOUSE_IsLocked();
    reg_cx = (uint16_t)static_cast<int16_t>(locked ? mouse.mickey_x : 0);
    reg_dx = (uint16_t)static_cast<int16_t>(locked ? mouse.mickey_y : 0);
    mouse.mickey_x = 0;
    mouse.mickey_y = 0;
    Mouse_Used();
}

// Reference: http://www.ctyme.com/intr/int-33.htm
static Bitu INT33_Handler(void) {
//  LOG(LOG_MOUSE,LOG_NORMAL)("MOUSE: %04X %X %X %d %d",reg_ax,reg_bx,reg_cx,POS_X,POS_Y);
    switch (reg_ax) {
    case 0x00:  /* MS MOUSE - RESET DRIVER AND READ STATUS */
        Mouse_ResetHardware();
        goto software_reset;
    case 0x01:  /* MS MOUSE v1.0+ - SHOW MOUSE CURSOR */
        if (mouse.hidden) mouse.hidden--;
        mouse.updateRegion_y[1] = -1; //offscreen
        DrawCursor();
        if(!mouse.hidden) Mouse_Used();
        break;
    case 0x02:  /* MS MOUSE v1.0+ - HIDE MOUSE CURSOR */
        {
            if (CurMode->type != M_TEXT) RestoreCursorBackground();
            else RestoreCursorBackgroundText();
            if (mouse.hidden == 0) mouse.hidden_at = PIC_FullIndex();
            mouse.hidden++;
            if (usesystemcursor&&!MOUSE_IsLocked()) SDL_ShowCursor(SDL_DISABLE);
        }
        break;
    case 0x03:  /* MS MOUSE v1.0+ - RETURN POSITION AND BUTTON STATUS */
        reg_bl=mouse.buttons;
        reg_bh=GetWheel8bit(); /* CuteMouse wheel extension */
        reg_cx=POS_X;
        reg_dx=POS_Y;
        mouse.first_range_setx = false;
        mouse.first_range_sety = false;
        if (en_int33_hide_if_polling) int33_last_poll = PIC_FullIndex();
        Mouse_Used();
        break;
    case 0x04:  /* MS MOUSE v1.0+ - POSITION MOUSE CURSOR */
        /* If position isn't different from current position
         * don't change it then. (as position is rounded so numbers get
         * lost when the rounded number is set) (arena/simulation Wolf) */
        if ((int16_t)reg_cx >= mouse.max_x) mouse.x = static_cast<float>(mouse.max_x);
        else if (mouse.min_x >= (int16_t)reg_cx) mouse.x = static_cast<float>(mouse.min_x);
        else if ((int16_t)reg_cx != POS_X) mouse.x = static_cast<float>(reg_cx);

        if ((int16_t)reg_dx >= mouse.max_y) mouse.y = static_cast<float>(mouse.max_y);
        else if (mouse.min_y >= (int16_t)reg_dx) mouse.y = static_cast<float>(mouse.min_y);
        else if ((int16_t)reg_dx != POS_Y) mouse.y = static_cast<float>(reg_dx);
        DrawCursor();
        if (en_int33_hide_if_polling) int33_last_poll = PIC_FullIndex();
        break;
    case 0x05:  /* MS MOUSE v1.0+ - RETURN BUTTON PRESS DATA */
        {
            uint16_t but = reg_bx;
            if (but==0xFFFF){
			    /* CuteMouse wheel extension */
			    reg_bx=GetWheel16bit();
			    reg_cx=mouse.last_scrolled_x;
			    reg_dx=mouse.last_scrolled_y;
			} else {
				reg_ax=mouse.buttons;
				if (but>=MOUSE_BUTTONS) but = MOUSE_BUTTONS - 1;
				reg_cx=mouse.last_pressed_x[but];
				reg_dx=mouse.last_pressed_y[but];
				reg_bx=mouse.times_pressed[but];
				mouse.times_pressed[but]=0;
			}
            if (en_int33_hide_if_polling) int33_last_poll = PIC_FullIndex();
        }
        Mouse_Used();
        break;
    case 0x06:  /* MS MOUSE v1.0+ - RETURN BUTTON RELEASE DATA */
        {
            uint16_t but = reg_bx;
            if (but==0xFFFF){
			    /* CuteMouse wheel extension */
			    reg_bx=(mouse.wheel >= 0) ? mouse.wheel : (0x10000 + mouse.wheel);
			    reg_cx=mouse.last_scrolled_x;
			    reg_dx=mouse.last_scrolled_y;
			    mouse.wheel=0;
			} else {
				reg_ax=mouse.buttons;
				if (but>=MOUSE_BUTTONS) but = MOUSE_BUTTONS - 1;
				reg_cx=mouse.last_released_x[but];
				reg_dx=mouse.last_released_y[but];
				reg_bx=mouse.times_released[but];
				mouse.times_released[but]=0;
			}
            if (en_int33_hide_if_polling) int33_last_poll = PIC_FullIndex();
        }
        Mouse_Used();
        break;
    case 0x07:  /* MS MOUSE v1.0+ - DEFINE HORIZONTAL CURSOR RANGE */
        {
            //Lemmings sets 1-640 and wants that. Ironseed sets 0-640 but doesn't like 640
            //Ironseed works if newvideo mode with mode 13 sets 0-639
            //Larry 6 actually wants newvideo mode with mode 13 to set it to 0-319
            int16_t max, min;
            if ((int16_t)reg_cx < (int16_t)reg_dx) { min = (int16_t)reg_cx; max = (int16_t)reg_dx; }
            else { min = (int16_t)reg_dx; max = (int16_t)reg_cx; }
            mouse.min_x = min;
            mouse.max_x = max;
            /* Battle Chess wants this */
            if (mouse.x > mouse.max_x) mouse.x = mouse.max_x;
            if (mouse.x < mouse.min_x) mouse.x = mouse.min_x;
            /* Or alternatively this:
            mouse.x = (mouse.max_x - mouse.min_x + 1)/2;*/
            LOG(LOG_MOUSE, LOG_NORMAL)("Define Horizontal range min:%d max:%d", min, max);

            /* NTS: The mouse in VESA BIOS modes would ideally start with the x and y ranges
             *      that fit the screen, but I'm not so sure mouse drivers even pay attention
             *      to VESA BIOS modes so it's not certain what comes out. However some
             *      demoscene productions like "Aqua" will set their own mouse range and draw
             *      their own cursor. The menu in "Aqua" will set up 640x480 256-color mode
             *      and then set a mouse range of x=0-1279 and y=0-479. Using the FIRST range
             *      set after mode set is the only way to make sure mouse pointer integration
             *      tracks the guest pointer properly. */
            if (mouse.first_range_setx || mouse.buttons == 0) {
                if (mouse.min_x == 0 && mouse.max_x > 0) {
                    // most games redefine the range so they can use a saner range matching the screen
                    int16_t nval = mouse.max_x;

                    if (CurMode->type == M_TEXT) {
                        // Text is reported as if each row is 8 lines high (CGA compat) even if EGA 14-line
                        // or VGA 16-line, and 8 pixels wide even if EGA/VGA 9-pixels/char is enabled.
                        //
                        // Apply sanity rounding.
                        //
                        // FreeDOS EDIT: The max is set to just under 640x400, so that the cursor only has
                        //               room for ONE PIXEL in the last row and column.
                        if (nval >= ((int16_t)(CurMode->twidth*8) - 32) && nval <= ((int16_t)(CurMode->twidth*8) + 32))
                            nval = (int16_t)CurMode->twidth*8;
                    }
                    else {
                        // Apply sanity rounding.
                        //
                        // Daggerfall: Sets max x to 310 instead of 320, probably to prevent drawing the cursor
                        //             partially offscreen. */
                        if (nval >= ((int16_t)CurMode->swidth - 32) && nval <= ((int16_t)CurMode->swidth + 32))
                            nval = (int16_t)CurMode->swidth;
                        else if (nval >= (((int16_t)CurMode->swidth - 32) * 2) && nval <= (((int16_t)CurMode->swidth + 32) * 2))
                            nval = (int16_t)CurMode->swidth * 2;
                    }

                    if (mouse.max_screen_x != nval) {
                        mouse.max_screen_x = nval;
                        LOG(LOG_MOUSE, LOG_NORMAL)("Define Horizontal range min:%d max:%d defines the bounds of the screen", min, max);
                    }
                }
                mouse.first_range_setx = false;
            }
        }
        break;
    case 0x08:  /* MS MOUSE v1.0+ - DEFINE VERTICAL CURSOR RANGE */
        {
            // Not sure what to take instead of the CurMode (see case 0x07 as well)
            // especially the cases where sheight= 400 and we set it with the mouse_reset to 200
            // disabled it at the moment. Seems to break Syndicate which wants 400 in mode 13
            int16_t max, min;
            if ((int16_t)reg_cx < (int16_t)reg_dx) { min = (int16_t)reg_cx; max = (int16_t)reg_dx; }
            else { min = (int16_t)reg_dx; max = (int16_t)reg_cx; }
            mouse.min_y = min;
            mouse.max_y = max;
            /* Battle Chess wants this */
            if (mouse.y > mouse.max_y) mouse.y = mouse.max_y;
            if (mouse.y < mouse.min_y) mouse.y = mouse.min_y;
            /* Or alternatively this:
            mouse.y = (mouse.max_y - mouse.min_y + 1)/2;*/
            LOG(LOG_MOUSE, LOG_NORMAL)("Define Vertical range min:%d max:%d", min, max);

            /* NTS: The mouse in VESA BIOS modes would ideally start with the x and y ranges
             *      that fit the screen, but I'm not so sure mouse drivers even pay attention
             *      to VESA BIOS modes so it's not certain what comes out. However some
             *      demoscene productions like "Aqua" will set their own mouse range and draw
             *      their own cursor. The menu in "Aqua" will set up 640x480 256-color mode
             *      and then set a mouse range of x=0-1279 and y=0-479. Using the FIRST range
             *      set after mode set is the only way to make sure mouse pointer integration
             *      tracks the guest pointer properly. */
            if (mouse.first_range_sety || mouse.buttons == 0) {
                if (mouse.min_y == 0 && mouse.max_y > 0) {
                    // most games redefine the range so they can use a saner range matching the screen
                    int16_t nval = mouse.max_y;

                    if (CurMode->type == M_TEXT) {
                        // Text is reported as if each row is 8 lines high (CGA compat) even if EGA 14-line
                        // or VGA 16-line, and 8 pixels wide even if EGA/VGA 9-pixels/char is enabled.
                        //
                        // Apply sanity rounding.
                        //
                        // FreeDOS EDIT: The max is set to just under 640x400, so that the cursor only has
                        //               room for ONE PIXEL in the last row and column.
                        if (nval >= ((int16_t)(CurMode->theight*8) - 32) && nval <= ((int16_t)(CurMode->theight*8) + 32))
                            nval = (int16_t)CurMode->theight*8;
                    }
                    else {
                        // Apply sanity rounding.
                        //
                        // Daggerfall: Sets max x to 310 instead of 320, probably to prevent drawing the cursor
                        //             partially offscreen. */
                        if (nval >= ((int16_t)CurMode->sheight - 32) && nval <= ((int16_t)CurMode->sheight + 32))
                            nval = (int16_t)CurMode->sheight;
                        else if (nval >= (((int16_t)CurMode->sheight - 32) * 2) && nval <= (((int16_t)CurMode->sheight + 32) * 2))
                            nval = (int16_t)CurMode->sheight * 2;
                    }

                    if (mouse.max_screen_y != nval) {
                        mouse.max_screen_y = nval;
                        LOG(LOG_MOUSE, LOG_NORMAL)("Define Vertical range min:%d max:%d defines the bounds of the screen", min, max);
                    }
                }
                mouse.first_range_sety = false;
            }
        }
        break;
    case 0x09:  /* MS MOUSE v3.0+ - DEFINE GRAPHICS CURSOR */
        {
            PhysPt src = SegPhys(es) + reg_dx;
            MEM_BlockRead(src, userdefScreenMask, CURSORY * 2);
            MEM_BlockRead(src + CURSORY * 2, userdefCursorMask, CURSORY * 2);
            mouse.screenMask = userdefScreenMask;
            mouse.cursorMask = userdefCursorMask;
            mouse.hotx = (int16_t)reg_bx;
            mouse.hoty = (int16_t)reg_cx;
            mouse.cursorType = 2;
            DrawCursor();
            break;
        }
    case 0x0a:  /* MS MOUSE v3.0+ - DEFINE TEXT CURSOR */
        mouse.cursorType = (reg_bx ? 1 : 0);
        mouse.textAndMask = reg_cx;
        mouse.textXorMask = reg_dx;
        if (reg_bx) {
            INT10_SetCursorShape(reg_cl, reg_dl);
            LOG(LOG_MOUSE, LOG_NORMAL)("Hardware Text cursor selected");
        }
        DrawCursor();
        break;
    case 0x0b:  /* MS MOUSE v1.0+ - READ MOTION COUNTERS */
        Mouse_Read_Motion_Data();
        break;
    case 0x0c:  /* MS MOUSE v1.0+ - DEFINE INTERRUPT SUBROUTINE PARAMETERS */
        mouse.sub_mask = reg_cx;
        mouse.sub_seg = SegValue(es);
        mouse.sub_ofs = reg_dx;
        if(mouse.sub_mask) Mouse_Used();
        break;
    case 0x0d:  /* MS MOUSE v1.0+ - LIGHT PEN EMULATION ON */
        LOG(LOG_MOUSE, LOG_ERROR)("Mouse light pen emulation on not implemented");
        break;
    case 0x0e:  /* MS MOUSE v1.0+ - LIGHT PEN EMULATION OFF */
        LOG(LOG_MOUSE, LOG_ERROR)("Mouse light pen emulation off not implemented");
        break;
    case 0x0f:  /* MS MOUSE v1.0+ - DEFINE MICKEY/PIXEL RATIO */
        Mouse_SetMickeyPixelRate((int16_t)reg_cx, (int16_t)reg_dx);
        break;
    case 0x10:  /* MS MOUSE v1.0+ - DEFINE SCREEN REGION FOR UPDATING */
        mouse.updateRegion_x[0] = (int16_t)reg_cx;
        mouse.updateRegion_y[0] = (int16_t)reg_dx;
        mouse.updateRegion_x[1] = (int16_t)reg_si;
        mouse.updateRegion_y[1] = (int16_t)reg_di;
        DrawCursor();
        break;
    case 0x11:  /* Genius Mouse 9.06 - GET NUMBER OF BUTTONS */
        reg_ax = 0x574D; /* Identifier for detection purposes */
		reg_bx = 0;      /* Reserved capabilities flags */
		reg_cx = 1;      /* Wheel is supported */
		/* Previous implementation provided Genius mouse-specific function to get
		   number of buttons (https://sourceforge.net/p/dosbox/patches/32/), it was
		   returning 0xffff in reg_ax and number of buttons in reg_bx; I suppose
		   the CuteMouse extensions are more useful */
        break;
    case 0x12:  /* MS MOUSE - SET LARGE GRAPHICS CURSOR BLOCK */
        LOG(LOG_MOUSE, LOG_ERROR)("Set large graphics cursor block not implemented");
        break;
    case 0x13:  /* MS MOUSE v5.0+ - DEFINE DOUBLE-SPEED THRESHOLD */
        mouse.doubleSpeedThreshold = (reg_dx ? reg_dx : 64);
        break;
    case 0x14:  /* MS MOUSE v3.0+ - EXCHANGE INTERRUPT SUBROUTINES */
        {
            uint16_t oldSeg = mouse.sub_seg;
            uint16_t oldOfs = mouse.sub_ofs;
            uint16_t oldMask = mouse.sub_mask;
            // Set new values
            mouse.sub_mask = reg_cx;
            mouse.sub_seg = SegValue(es);
            mouse.sub_ofs = reg_dx;
            // Return old values
            reg_cx = (uint16_t)oldMask;
            reg_dx = (uint16_t)oldOfs;
            SegSet16(es, oldSeg);
        }
        break;
    case 0x15:  /* MS MOUSE v6.0+ - RETURN DRIVER STORAGE REQUIREMENTS */
        reg_bx = sizeof(mouse);
        break;
    case 0x16:  /* MS MOUSE v6.0+ - SAVE DRIVER STATE */
        {
            LOG(LOG_MOUSE, LOG_NORMAL)("Saving driver state...");
            PhysPt dest = SegPhys(es) + reg_dx;
            MEM_BlockWrite(dest, &mouse, sizeof(mouse));
        }
        break;
    case 0x17:  /* MS MOUSE v6.0+ - RESTORE DRIVER STATE */
        {
            LOG(LOG_MOUSE, LOG_NORMAL)("Loading driver state...");
            PhysPt src = SegPhys(es) + reg_dx;
            MEM_BlockRead(src, &mouse, sizeof(mouse));
            break;
        }
    case 0x18:  /* MS MOUSE v6.0+ - SET ALTERNATE MOUSE USER HANDLER */
        LOG(LOG_MOUSE, LOG_ERROR)("Set alternate mouse user handler not implemented");
        break;
    case 0x19:  /* MS MOUSE v6.0+ - RETURN USER ALTERNATE INTERRUPT VECTOR */
        LOG(LOG_MOUSE, LOG_ERROR)("Return user alternate interrupt vector not implemented");
        break;
    case 0x1a:  /* MS MOUSE v6.0+ - SET MOUSE SENSITIVITY */
        // ToDo : double mouse speed value
        Mouse_SetSensitivity(reg_bx, reg_cx, reg_dx);
        LOG(LOG_MOUSE, LOG_NORMAL)("Set sensitivity used with %d %d (%d)", reg_bx, reg_cx, reg_dx);
        break;
    case 0x1b:  /* MS MOUSE v6.0+ - RETURN MOUSE SENSITIVITY */
        reg_bx = mouse.senv_x_val;
        reg_cx = mouse.senv_y_val;
        reg_dx = mouse.dspeed_val;

        LOG(LOG_MOUSE, LOG_NORMAL)("Get sensitivity %d %d", reg_bx, reg_cx);
        break;
    case 0x1c:  /* MS MOUSE v6.0+ - SET INTERRUPT RATE */
        /* Can't really set a rate this is host determined */
        break;
    case 0x1d:  /* MS MOUSE v6.0+ - DEFINE DISPLAY PAGE NUMBER */
        mouse.page = reg_bl;
        break;
    case 0x1e:  /* MS MOUSE v6.0+ - RETURN DISPLAY PAGE NUMBER */
        reg_bx = mouse.page;
        break;
    case 0x1f:  /* MS MOUSE v6.0+ - DISABLE MOUSE DRIVER */
        /* ES:BX old mouse driver Zero at the moment TODO */
        reg_bx = 0;
        SegSet16(es, 0);
        mouse.enabled = false; /* Just for reporting not doing a thing with it */
        mouse.oldhidden = mouse.hidden;
        if (!mouse.hidden) mouse.hidden_at = PIC_FullIndex();
        mouse.hidden = 1;
        break;
    case 0x20:  /* MS MOUSE v6.0+ - ENABLE MOUSE DRIVER */
        mouse.enabled = true;
        mouse.hidden = mouse.oldhidden;
        break;
    case 0x21:  /* MS MOUSE v6.0+ - SOFTWARE RESET */
    software_reset:
        extern bool Mouse_Drv;
        if (Mouse_Drv) {
            reg_ax = 0xffff;
            reg_bx = MOUSE_BUTTONS;
            Mouse_Reset();
            Mouse_Used();
            AUX_INT33_Takeover();
            LOG(LOG_MOUSE, LOG_NORMAL)("INT 33h reset");
        }
        break;
    case 0x22:  /* MS MOUSE v6.0+ - SET LANGUAGE FOR MESSAGES */
            /*
             *                        Values for mouse driver language:
             *
             *                        00h     English
             *                        01h     French
             *                        02h     Dutch
             *                        03h     German
             *                        04h     Swedish
             *                        05h     Finnish
             *                        06h     Spanish
             *                        07h     Portuguese
             *                        08h     Italian
             *
             */
        mouse.language = reg_bx;
        break;
    case 0x23:  /* MS MOUSE v6.0+ - GET LANGUAGE FOR MESSAGES */
        reg_bx = mouse.language;
        break;
    case 0x24:  /* MS MOUSE v6.26+ - GET SOFTWARE VERSION, MOUSE TYPE, AND IRQ NUMBER */
	// TODO: If PC-98 mode, return type 0x01 bus mouse, and bus mouse IRQ.
	//       If emulating a serial mouse, return type 0x02 and serial port IRQ.
        reg_bx = 0x805;   //Version 8.05 woohoo 
        reg_ch = 0x04;    /* PS/2 type */
        reg_cl = 0;       /* PS/2 mouse == 0, for any other type, this would be the IRQ */
        break;
    case 0x25:  /* MS MOUSE v6.26+ - GET GENERAL DRIVER INFORMATION */
	// TODO: According to PC sourcebook reference
	//       Returns:
	//       AH = status
	//         bit 7 driver type: 1=sys 0=com
	//         bit 6: 0=non-integrated 1=integrated mouse driver
	//         bits 4-5: cursor type  00=software text cursor 01=hardware text cursor 1X=graphics cursor
	//         bits 0-3: Function 28 mouse interrupt rate
	//       AL = Number of MDDS (?)
	//       BX = fCursor lock
	//       CX = FinMouse code
	//       DX = fMouse busy
        LOG(LOG_MOUSE, LOG_ERROR)("Get general driver information not implemented");
        break;
    case 0x26:  /* MS MOUSE v6.26+ - GET MAXIMUM VIRTUAL COORDINATES */
        reg_bx = (mouse.enabled ? 0x0000 : 0xffff);
        reg_cx = (uint16_t)mouse.max_x;
        reg_dx = (uint16_t)mouse.max_y;
        break;
    case 0x27:  /* MS MOUSE v7.01+ - GET SCREEN/CURSOR MASKS AND MICKEY COUNTS */
        reg_ax = mouse.textAndMask;
        reg_bx = mouse.textXorMask;
        Mouse_Read_Motion_Data();
        break;
    case 0x28:  /* MS MOUSE v7.0+ - SET VIDEO MODE */
	// TODO: According to PC sourcebook
	//       Entry:
	//       CX = Requested video mode
	//       DX = Font size, 0 for default
	//       Returns:
	//       DX = 0 on success, nonzero (requested video mode) if not
        LOG(LOG_MOUSE, LOG_ERROR)("Set video mode not implemented");
        break;
    case 0x29: /* MS MOUSE v7.0+ - ENUMERATE VIDEO MODES */
	// TODO: According to PC sourcebook
	//       Entry:
	//       CX = 0 for first, != 0 for next
	//       Exit:
	//       BX:DX = named string far ptr
	//       CX = video mode number
        LOG(LOG_MOUSE, LOG_ERROR)("Enumerate video modes not implemented");
        break;
    case 0x2a:  /* MS MOUSE v7.02+ - GET CURSOR HOT SPOT */
        // NTS: Whoever compiled the INT 33 list in the Microsoft PC sourcebook made a big mistake
        //      here and started counting hexadecimal as decimal. This function is wrongly listed as INT 33h AH=30H.
        //      You can tell by the summary list which shows both hexadecimal and decimal side by side, this is
        //      listed as AH=30h (42). Decimal 42 as hexadecimal would be 0x2A (0x20 + 0x0A) == (32 + 10) == 42 == 0x2A.
        reg_al = (uint8_t)-mouse.hidden;    // Microsoft uses a negative byte counter for cursor visibility
        reg_bx = (uint16_t)mouse.hotx;
        reg_cx = (uint16_t)mouse.hoty;
        reg_dx = 0x04;    // PS/2 mouse type
        break;
    case 0x2b:  /* MS MOUSE v7.0+ - LOAD ACCELERATION PROFILES */
        LOG(LOG_MOUSE, LOG_ERROR)("Load acceleration profiles not implemented");
        break;
    case 0x2c:  /* MS MOUSE v7.0+ - GET ACCELERATION PROFILES */
        LOG(LOG_MOUSE, LOG_ERROR)("Get acceleration profiles not implemented");
        break;
    case 0x2d:  /* MS MOUSE v7.0+ - SELECT ACCELERATION PROFILE */
        LOG(LOG_MOUSE, LOG_ERROR)("Select acceleration profile not implemented");
        break;
    case 0x2e:  /* MS MOUSE v8.10+ - SET ACCELERATION PROFILE NAMES */
        LOG(LOG_MOUSE, LOG_ERROR)("Set acceleration profile names not implemented");
        break;
    case 0x2f:  /* MS MOUSE v7.02+ - MOUSE HARDWARE RESET */
        LOG(LOG_MOUSE, LOG_ERROR)("INT 33 AX=2F mouse hardware reset not implemented");
        break;
    case 0x30:  /* MS MOUSE v7.04+ - GET/SET BallPoint INFORMATION */
        LOG(LOG_MOUSE, LOG_ERROR)("Get/set BallPoint information not implemented");
        break;
    case 0x31:  /* MS MOUSE v7.05+ - GET CURRENT MINIMUM/MAXIMUM VIRTUAL COORDINATES */
        reg_ax = (uint16_t)mouse.min_x;
        reg_bx = (uint16_t)mouse.min_y;
        reg_cx = (uint16_t)mouse.max_x;
        reg_dx = (uint16_t)mouse.max_y;
        break;
    case 0x32:  /* MS MOUSE v7.05+ - GET ACTIVE ADVANCED FUNCTIONS */
        LOG(LOG_MOUSE, LOG_ERROR)("Get active advanced functions not implemented");
        break;
    case 0x33:  /* MS MOUSE v7.05+ - GET SWITCH SETTINGS AND ACCELERATION PROFILE DATA */
        LOG(LOG_MOUSE, LOG_ERROR)("Get switch settings and acceleration profile data not implemented");
        break;
    case 0x34:  /* MS MOUSE v8.0+ - GET INITIALIZATION FILE */
        LOG(LOG_MOUSE, LOG_ERROR)("Get initialization file not implemented");
        break;
    case 0x35:  /* MS MOUSE v8.10+ - LCD SCREEN LARGE POINTER SUPPORT */
        LOG(LOG_MOUSE, LOG_ERROR)("LCD screen large pointer support not implemented");
        break;
    case 0x4d:  /* MS MOUSE - RETURN POINTER TO COPYRIGHT STRING */
        LOG(LOG_MOUSE, LOG_ERROR)("Return pointer to copyright string not implemented");
        break;
    case 0x6d:  /* MS MOUSE - GET VERSION STRING */
        LOG(LOG_MOUSE, LOG_ERROR)("Get version string not implemented");
        break;
    case 0x53C1:  /* Logitech CyberMan */
        LOG(LOG_MOUSE, LOG_NORMAL)("Mouse function 53C1 for Logitech CyberMan called. Ignored by regular mouse driver.");
        break;
    default:
        LOG(LOG_MOUSE, LOG_ERROR)("Mouse Function %04X not implemented!", reg_ax);
        break;
    }
    return CBRET_NONE;
}

static Bitu MOUSE_BD_Handler(void) {
    // the stack contains offsets to register values
    uint16_t raxpt=real_readw(SegValue(ss),reg_sp+0x0a);
    uint16_t rbxpt=real_readw(SegValue(ss),reg_sp+0x08);
    uint16_t rcxpt=real_readw(SegValue(ss),reg_sp+0x06);
    uint16_t rdxpt=real_readw(SegValue(ss),reg_sp+0x04);

    // read out the actual values, registers ARE overwritten
    uint16_t rax=real_readw(SegValue(ds),raxpt);
    reg_ax=rax;
    reg_bx=real_readw(SegValue(ds),rbxpt);
    reg_cx=real_readw(SegValue(ds),rcxpt);
    reg_dx=real_readw(SegValue(ds),rdxpt);
//  LOG_MSG("MOUSE BD: %04X %X %X %X %d %d",reg_ax,reg_bx,reg_cx,reg_dx,POS_X,POS_Y);
    
    // some functions are treated in a special way (additional registers)
    switch (rax) {
        case 0x09:  /* Define GFX Cursor */
        case 0x16:  /* Save driver state */
        case 0x17:  /* load driver state */
            SegSet16(es,SegValue(ds));
            break;
        case 0x0c:  /* Define interrupt subroutine parameters */
        case 0x14:  /* Exchange event-handler */ 
            if (reg_bx!=0) SegSet16(es,reg_bx);
            else SegSet16(es,SegValue(ds));
            break;
        case 0x10:  /* Define screen region for updating */
            reg_cx=real_readw(SegValue(ds),rdxpt);
            reg_dx=real_readw(SegValue(ds),rdxpt+2);
            reg_si=real_readw(SegValue(ds),rdxpt+4);
            reg_di=real_readw(SegValue(ds),rdxpt+6);
            break;
        default:
            break;
    }

    INT33_Handler();

    // save back the registers, too
    real_writew(SegValue(ds),raxpt,reg_ax);
    real_writew(SegValue(ds),rbxpt,reg_bx);
    real_writew(SegValue(ds),rcxpt,reg_cx);
    real_writew(SegValue(ds),rdxpt,reg_dx);
    switch (rax) {
        case 0x1f:  /* Disable Mousedriver */
            real_writew(SegValue(ds),rbxpt,SegValue(es));
            break;
        case 0x14: /* Exchange event-handler */ 
            real_writew(SegValue(ds),rcxpt,SegValue(es));
            break;
        default:
            break;
    }

    reg_ax=rax;
    return CBRET_NONE;
}

static Bitu INT74_Handler(void) {
    if (mouse.events>0 && !mouse.in_UIR) {
        mouse.events--;

        /* INT 33h emulation: HERE within the IRQ 12 handler is the appropriate place to
         * redraw the cursor. OSes like Windows 3.1 expect real-mode code to do it in
         * response to IRQ 12, not "out of the blue" from the SDL event handler like
         * the original DOSBox code did it. Doing this allows the INT 33h emulation
         * to draw the cursor while not causing Windows 3.1 to crash or behave
         * erratically. */
        if (AllowINT33RMAccess() && en_int33) DrawCursor();

        /* Check for an active Interrupt Handler that will get called */
        if (AllowINT33RMAccess() && (mouse.sub_mask & mouse.event_queue[mouse.events].type)) {
            reg_ax=mouse.event_queue[mouse.events].type
                  | (!MOUSE_IsLocked() ? MOUSE_ABSOLUTE & mouse.sub_mask : 0);
            reg_bl=mouse.event_queue[mouse.events].buttons;
            reg_bh=GetWheel8bit(); /* CuteMouse wheel extension */
            reg_cx=(uint16_t)POS_X;
            reg_dx=(uint16_t)POS_Y;
            reg_si=(uint16_t)static_cast<int16_t>(mouse.mickey_x);
            reg_di=(uint16_t)static_cast<int16_t>(mouse.mickey_y);
            CPU_Push16(RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
            CPU_Push16(RealOff(CALLBACK_RealPointer(int74_ret_callback))+7);
            CPU_Push16(RealSeg(uir_callback));
            CPU_Push16(RealOff(uir_callback));
            CPU_Push16(mouse.sub_seg);
            CPU_Push16(mouse.sub_ofs);
            mouse.in_UIR = true;
        } else if (useps2callback) {
            CPU_Push16(RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
            CPU_Push16(RealOff(CALLBACK_RealPointer(int74_ret_callback)));
            DoPS2Callback(mouse.event_queue[mouse.events].buttons, static_cast<int16_t>(mouse.ps2x), static_cast<int16_t>(mouse.ps2y));
        } else {
            SegSet16(cs, RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
            reg_ip = RealOff(CALLBACK_RealPointer(int74_ret_callback));
        }
    } else {
        SegSet16(cs, RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
        reg_ip = RealOff(CALLBACK_RealPointer(int74_ret_callback));
    }
    return CBRET_NONE;
}

Bitu INT74_Ret_Handler(void) {
    if (mouse.events) {
        if (!mouse.timer_in_progress) {
            mouse.timer_in_progress = true;
            PIC_AddEvent(MOUSE_Limit_Events,MOUSE_DELAY);
        }
    }
    return CBRET_NONE;
}

Bitu UIR_Handler(void) {
    mouse.in_UIR = false;
    return CBRET_NONE;
}

bool MouseTypeNone();

void MOUSE_OnReset(Section *sec) {
    (void)sec;//UNUSED
    if (IS_PC98_ARCH)
        MOUSE_IRQ = 13; // PC-98 standard
    else if (!enable_slave_pic)
        MOUSE_IRQ = 0;
    else
        MOUSE_IRQ = 12; // IBM PC/AT standard

    if (MOUSE_IRQ != 0)
        PIC_SetIRQMask(MOUSE_IRQ,true);
}

void MOUSE_ShutDown(Section *sec) {
    (void)sec;//UNUSED
}

void BIOS_PS2MOUSE_ShutDown(Section *sec) {
    (void)sec;//UNUSED
}

void BIOS_PS2Mouse_Startup(Section *sec) {
    (void)sec;//UNUSED
    Section_prop *section=static_cast<Section_prop *>(control->GetSection("dos"));

    /* NTS: This assumes MOUSE_Init() is called after KEYBOARD_Init() */
    en_bios_ps2mouse = section->Get_bool("biosps2");

    if (!enable_slave_pic || machine == MCH_PCJR) return;

    if (!en_bios_ps2mouse) return;

    if (MouseTypeNone()) {
        LOG(LOG_MOUSE, LOG_WARN)("INT 15H PS/2 emulation NOT enabled. biosps2=1 but mouse type=none");
    }
    else {
        LOG(LOG_MOUSE, LOG_NORMAL)("INT 15H PS/2 emulation enabled");
        bios_enable_ps2();
    }

    ps2_callback_save_regs = section->Get_bool("int15 mouse callback does not preserve registers");

    // Callback for ps2 irq
    call_int74=CALLBACK_Allocate();
    CALLBACK_Setup(call_int74,&INT74_Handler,CB_IRQ12,"int 74");
    // pseudocode for CB_IRQ12:
    //  sti
    //  push ds
    //  push es
    //  pushad
    //  sti
    //  callback INT74_Handler
    //      ps2 or user callback if requested
    //      otherwise jumps to CB_IRQ12_RET
    //  push ax
    //  mov al, 0x20
    //  out 0xa0, al
    //  out 0x20, al
    //  pop ax
    //  cld
    //  retf

    int74_ret_callback=CALLBACK_Allocate();
    CALLBACK_Setup(int74_ret_callback,&INT74_Ret_Handler,CB_IRQ12_RET,"int 74 ret");
    // pseudocode for CB_IRQ12_RET:
    //  cli
    //  mov al, 0x20
    //  out 0xa0, al
    //  out 0x20, al
    //  callback INT74_Ret_Handler
    //  popad
    //  pop es
    //  pop ds
    //  iret

    if (MOUSE_IRQ != 0) {
        uint8_t hwvec=(MOUSE_IRQ>7)?(0x70+MOUSE_IRQ-8):(0x8+MOUSE_IRQ);
        RealSetVec(hwvec,CALLBACK_RealPointer(call_int74));
    }

    // Callback for ps2 user callback handling
    useps2callback = false; ps2callbackinit = false;
    if (call_ps2 == 0)
        call_ps2 = CALLBACK_Allocate();
    CALLBACK_Setup(call_ps2,&PS2_Handler,CB_RETF,"ps2 bios callback");
    ps2_callback=CALLBACK_RealPointer(call_ps2);

    // Callback for mouse user routine return
    if (call_uir == 0)
        call_uir = CALLBACK_Allocate();
    CALLBACK_Setup(call_uir,&UIR_Handler,CB_RETF_CLI,"mouse uir ret");
    uir_callback=CALLBACK_RealPointer(call_uir);
}

void MOUSE_Startup(Section *sec) {
    (void)sec;//UNUSED
    Section_prop *section=static_cast<Section_prop *>(control->GetSection("dos"));
	Section_prop * pc98_section=static_cast<Section_prop *>(control->GetSection("pc98"));
    RealPt i33loc=0;
    extern bool serialMouseEmulated;

    if (MouseTypeNone() && !serialMouseEmulated) {
        return;
    }

    user_mouse_report_rate=section->Get_int("mouse report rate");
    UpdateMouseReportRate();

    en_int33_hide_if_intsub=section->Get_bool("int33 hide host cursor if interrupt subroutine");

    en_int33_hide_if_polling=section->Get_bool("int33 hide host cursor when polling");

    en_int33_pc98_show_graphics=pc98_section->Get_bool("pc-98 show graphics layer on initialize");

    en_vmware=section->Get_bool("vmware");

    en_int33=section->Get_bool("int33");
    if (!en_int33) {
        Mouse_Reset();
        Mouse_SetSensitivity(50,50,50);
        return;
    }

    cell_granularity_disable=section->Get_bool("int33 disable cell granularity");

    LOG(LOG_MOUSE, LOG_NORMAL)("INT 33H emulation enabled");
    if (en_int33_hide_if_polling)
        LOG(LOG_MOUSE, LOG_NORMAL)("INT 33H emulation will hide host cursor if polling");

    // Callback for mouse interrupt 0x33
    call_int33=CALLBACK_Allocate();

    // i33loc=RealMake(CB_SEG+1,(call_int33*CB_SIZE)-0x10);
    i33loc=RealMake(DOS_GetMemory(0x1,"i33loc")-1,0x10);
    CALLBACK_Setup(call_int33,&INT33_Handler,CB_MOUSE,Real2Phys(i33loc),"Mouse");

    // Wasteland needs low(seg(int33))!=0 and low(ofs(int33))!=0
    real_writed(0,0x33<<2,i33loc);

    call_mouse_bd=CALLBACK_Allocate();
    CALLBACK_Setup(call_mouse_bd,&MOUSE_BD_Handler,CB_RETF8,
        PhysMake(RealSeg(i33loc),RealOff(i33loc)+2),"MouseBD");
    // pseudocode for CB_MOUSE (including the special backdoor entry point):
    //  jump near i33hd
    //  callback MOUSE_BD_Handler
    //  retf 8
    //  label i33hd:
    //  callback INT33_Handler
    //  iret

    memset(&mouse,0,sizeof(mouse));

    if (!mouse.hidden) mouse.hidden_at = PIC_FullIndex();
    mouse.hidden = 1;

    mouse.timer_in_progress = false;
    mouse.mode = 0xFF; //Non existing mode
    mouse.scrollwheel = 0;

    mouse.sub_mask=0;
    mouse.sub_seg=0x6362;   // magic value
    mouse.sub_ofs=0;

    oldmouseX = oldmouseY = 0;
    mouse.ps2x = mouse.ps2y = 0;

    Mouse_ResetHardware();
    Mouse_Reset();
    Mouse_SetSensitivity(50,50,50);

    if (en_vmware) {
	    LOG(LOG_MISC,LOG_DEBUG)("Enabling VMware integration for mouse interface");
	    IO_RegisterReadHandler(VMWARE_PORT, &PortRead, IO_MD);
    }
}

void MOUSE_Init() {
    LOG(LOG_MOUSE, LOG_DEBUG)("Initializing mouse interface emulation");

    // TODO: We need a DOSBox shutdown callback, and we need a shutdown callback for when the DOS kernel begins to unload and on system reset
    AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(MOUSE_OnReset));
}

bool MOUSE_IsHidden()
{
    /* mouse is returned hidden IF hidden for more than 100ms.
     * rapidly hiding/showing should not signal hidden (FreeDOS EDIT.COM) */
    return static_cast<bool>(mouse.hidden) && (PIC_FullIndex() >= (mouse.hidden_at + 100));
}

bool MOUSE_IsBeingPolled()
{
    if (!en_int33_hide_if_polling)
        return false;

    return (PIC_FullIndex() < (int33_last_poll + 1000));
}

bool MOUSE_HasInterruptSub()
{
    if (!en_int33_hide_if_intsub)
        return false;

    return (mouse.sub_mask != 0);
}

// Basic VMware tools support, based on documentation from https://wiki.osdev.org/VMware_tools
// Mouse support tested using unofficial Windows 3.1 driver from https://github.com/NattyNarwhal/vmwmouse

static constexpr uint32_t VMWARE_MAGIC           = 0x564D5868u;    // magic number for all VMware calls

static constexpr uint16_t CMD_GETVERSION         = 10u;
static constexpr uint16_t CMD_ABSPOINTER_DATA    = 39u;
static constexpr uint16_t CMD_ABSPOINTER_STATUS  = 40u;
static constexpr uint16_t CMD_ABSPOINTER_COMMAND = 41u;

static constexpr uint32_t ABSPOINTER_ENABLE      = 0x45414552u;
static constexpr uint32_t ABSPOINTER_RELATIVE    = 0xF5u;
static constexpr uint32_t ABSPOINTER_ABSOLUTE    = 0x53424152u;

static constexpr uint8_t BUTTON_LEFT            = 0x20u;
static constexpr uint8_t BUTTON_RIGHT           = 0x10u;
static constexpr uint8_t BUTTON_MIDDLE          = 0x08u;

volatile bool vmware_mouse     = false;  // if true, VMware compatible driver has taken over the mouse

static uint8_t mouse_buttons    = 0;      // state of mouse buttons, in VMware format
static uint16_t mouse_x          = 0x8000; // mouse position X, in VMware format (scaled from 0 to 0xFFFF)
static uint16_t mouse_y          = 0x8000; // ditto
static int8_t mouse_wheel      = 0;
static bool mouse_updated    = false;

static int16_t mouse_diff_x     = 0;      // difference between host and guest mouse x coordinate (in host pixels)
static int16_t mouse_diff_y     = 0;      // ditto

static bool video_fullscreen = false;
static uint16_t video_res_x      = 1;      // resolution to which guest image is scaled, excluding black borders
static uint16_t video_res_y      = 1;
static uint16_t video_clip_x     = 0;      // clipping value - size of black border (one side)
static uint16_t video_clip_y     = 0;


class Section;

// Commands (requests) to the VMware hypervisor

static inline void CmdGetVersion() {
        reg_eax = 0; // FIXME: should we respond with something resembling VMware?
        reg_ebx = VMWARE_MAGIC;
}

static inline void CmdAbsPointerData() {
        reg_eax = mouse_buttons;
        reg_ebx = mouse_x;
        reg_ecx = mouse_y;
        reg_edx = (mouse_wheel >= 0) ? mouse_wheel : 256 + mouse_wheel;

        mouse_wheel = 0;
}

static inline void CmdAbsPointerStatus() {
        reg_eax = mouse_updated ? 4 : 0;
        mouse_updated = false;
}

static inline void CmdAbsPointerCommand() {

        switch (reg_ebx) {
        case ABSPOINTER_ENABLE:
                // can be safely ignored
                break;
        case ABSPOINTER_RELATIVE:
                vmware_mouse = false;
                if (!MOUSE_IsLocked()) SDL_ShowCursor(SDL_ENABLE);
                break;
        case ABSPOINTER_ABSOLUTE:
                vmware_mouse = true;
                if (!MOUSE_IsLocked()) SDL_ShowCursor(SDL_DISABLE);
                break;
        default:
                LOG_MSG("VMWARE: unknown mouse subcommand 0x%08x", reg_ebx);
                break;
        }
}

// IO port handling

static Bitu PortRead(Bitu port, Bitu iolen) {
    (void)port;
    (void)iolen;

        if (reg_eax != VMWARE_MAGIC)
                return 0;

        // LOG_MSG("VMWARE: called with EBX 0x%08x, ECX 0x%08x", reg_ebx, reg_ecx);

        switch (reg_cx) {
        case CMD_GETVERSION:
                CmdGetVersion();
                break;
        case CMD_ABSPOINTER_DATA:
                CmdAbsPointerData();
                break;
        case CMD_ABSPOINTER_STATUS:
                CmdAbsPointerStatus();
                break;
        case CMD_ABSPOINTER_COMMAND:
                CmdAbsPointerCommand();
                break;
        default:
                LOG_MSG("VMWARE: unknown command 0x%08x", reg_ecx);
                break;
        }

        return reg_ax;
}

// Notifications from external subsystems

void VMWARE_MouseButtonPressed(uint8_t button) {

        switch (button) {
        case 0:
                mouse_buttons |= BUTTON_LEFT;
                mouse_updated = true;
                break;
        case 1:
                mouse_buttons |= BUTTON_RIGHT;
                mouse_updated = true;
                break;
        case 2:
                mouse_buttons |= BUTTON_MIDDLE;
                mouse_updated = true;
                break;
        default:
                break;
        }
}

void VMWARE_MouseButtonReleased(uint8_t button) {

        switch (button) {
        case 0:
                mouse_buttons &= ~BUTTON_LEFT;
                mouse_updated = true;
                break;
        case 1:
                mouse_buttons &= ~BUTTON_RIGHT;
                mouse_updated = true;
                break;
        case 2:
                mouse_buttons &= ~BUTTON_MIDDLE;
                mouse_updated = true;
                break;
        default:
                break;
        }
}

void VMWARE_MousePosition(uint16_t pos_x, uint16_t pos_y) {

        float tmp_x;
        float tmp_y;

        if (video_fullscreen)
        {
                // We have to maintain the diffs (offsets) between host and guest
                // mouse positions; otherwise in case of clipped picture (like
                // 4:3 screen displayed on 16:9 fullscreen mode) we could have
                // an effect of 'sticky' borders if the user moves mouse outside
                // of the guest display area

                if (pos_x + mouse_diff_x < video_clip_x)
                        mouse_diff_x = video_clip_x - pos_x;
                else if (pos_x + mouse_diff_x >= video_res_x + video_clip_x)
                        mouse_diff_x = video_res_x + video_clip_x - pos_x - 1;

                if (pos_y + mouse_diff_y < video_clip_y)
                        mouse_diff_y = video_clip_y - pos_y;
                else if (pos_y + mouse_diff_y >= video_res_y + video_clip_y)
                        mouse_diff_y = video_res_y + video_clip_y - pos_y - 1;

                tmp_x = pos_x + mouse_diff_x - video_clip_x;
                tmp_y = pos_y + mouse_diff_y - video_clip_y;
        }
        else
        {
                tmp_x = pos_x > video_clip_x ? pos_x - video_clip_x : 0;
                tmp_y = pos_y > video_clip_y ? pos_y - video_clip_y : 0;
        }

        mouse_x = (std::min)(0xFFFFu, static_cast<uint32_t>(tmp_x * 0xFFFF / (video_res_x - 1) + 0.499));
        mouse_y = (std::min)(0xFFFFu, static_cast<uint32_t>(tmp_y * 0xFFFF / (video_res_y - 1) + 0.499));

        mouse_updated = true;
}

void VMWARE_MouseWheel(int32_t scroll) {
        if (scroll >= 255 || scroll + mouse_wheel >= 127)
                mouse_wheel = 127;
        else if (scroll <= -255 || scroll + mouse_wheel <= -127)
                mouse_wheel = -127;
        else
                mouse_wheel += scroll;

        mouse_updated = true;
}

void VMWARE_ScreenParams(uint16_t clip_x, uint16_t clip_y, uint16_t res_x, uint16_t res_y, bool fullscreen) {
        video_clip_x     = clip_x;
        video_clip_y     = clip_y;
        video_res_x      = res_x;
        video_res_y      = res_y;
        video_fullscreen = fullscreen;

        // Unfortunately, with seamless driver changing the window size can cause
        // mouse movement as a side-effect, this is not fun for games. Let's try
        // to at least minimize the effect.

        mouse_diff_x = clamp(static_cast<int32_t>(mouse_diff_x), -video_clip_x, static_cast<int32_t>(video_clip_x));
        mouse_diff_y = clamp(static_cast<int32_t>(mouse_diff_y), -video_clip_y, static_cast<int32_t>(video_clip_y));
}

//save state support
void *MOUSE_Limit_Events_PIC_Event = (void*)((uintptr_t)MOUSE_Limit_Events);


namespace
{
class SerializeMouse : public SerializeGlobalPOD
{
public:
	SerializeMouse() : SerializeGlobalPOD("Mouse")
	{}

private:
	virtual void getBytes(std::ostream& stream)
	{
		uint8_t screenMask_idx, cursorMask_idx;


		if( mouse.screenMask == defaultScreenMask ) screenMask_idx = 0x00;
		else if( mouse.screenMask == userdefScreenMask ) screenMask_idx = 0x01;

		if( mouse.cursorMask == defaultCursorMask ) cursorMask_idx = 0x00;
		else if( mouse.cursorMask == userdefCursorMask ) cursorMask_idx = 0x01;

		//*******************************************
		//*******************************************
		//*******************************************

		SerializeGlobalPOD::getBytes(stream);


		// - pure data
		WRITE_POD( &ps2cbseg, ps2cbseg );
		WRITE_POD( &ps2cbofs, ps2cbofs );
		WRITE_POD( &useps2callback, useps2callback );
		WRITE_POD( &ps2callbackinit, ps2callbackinit );
		
		WRITE_POD( &userdefScreenMask, userdefScreenMask );
		WRITE_POD( &userdefCursorMask, userdefCursorMask );


		// - near-pure data
		WRITE_POD( &mouse, mouse );

		// - pure data
		WRITE_POD( &gfxReg3CE, gfxReg3CE );
		WRITE_POD( &index3C4, index3C4 );
		WRITE_POD( &gfxReg3C5, gfxReg3C5 );

		//*******************************************
		//*******************************************
		//*******************************************

		// - reloc ptr
		WRITE_POD( &screenMask_idx, screenMask_idx );
		WRITE_POD( &cursorMask_idx, cursorMask_idx );
	}

	virtual void setBytes(std::istream& stream)
	{
		uint8_t screenMask_idx, cursorMask_idx;

		//*******************************************
		//*******************************************
		//*******************************************

		SerializeGlobalPOD::setBytes(stream);

		// - pure data
		READ_POD( &ps2cbseg, ps2cbseg );
		READ_POD( &ps2cbofs, ps2cbofs );
		READ_POD( &useps2callback, useps2callback );
		READ_POD( &ps2callbackinit, ps2callbackinit );
		
		READ_POD( &userdefScreenMask, userdefScreenMask );
		READ_POD( &userdefCursorMask, userdefCursorMask );


		// - near-pure data
		READ_POD( &mouse, mouse );


		// - pure data
		READ_POD( &gfxReg3CE, gfxReg3CE );
		READ_POD( &index3C4, index3C4 );
		READ_POD( &gfxReg3C5, gfxReg3C5 );

		//*******************************************
		//*******************************************
		//*******************************************

		// - reloc ptr
		READ_POD( &screenMask_idx, screenMask_idx );
		READ_POD( &cursorMask_idx, cursorMask_idx );


		if( screenMask_idx == 0x00 ) mouse.screenMask = defaultScreenMask;
		else if( screenMask_idx == 0x01 ) mouse.screenMask = userdefScreenMask;

		if( cursorMask_idx == 0x00 ) mouse.cursorMask = defaultCursorMask;
		else if( cursorMask_idx == 0x01 ) mouse.cursorMask = userdefCursorMask;

		//*******************************************
		//*******************************************
		//*******************************************

		// reset
		oldmouseX = static_cast<int16_t>(mouse.x);
		oldmouseY = static_cast<int16_t>(mouse.y);
	}
} dummy;
}
