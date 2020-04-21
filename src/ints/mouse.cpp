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

/* TODO: If biosps2=true and aux=false, also allow an option (default disabled)
 *       where if set, we don't bother to fire IRQ 12 at all but simply call the
 *       device callback directly. */

#include <string.h>
#include <math.h>


#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "cpu.h"
#include "mouse.h"
#include "pic.h"
#include "inout.h"
#include "int10.h"
#include "bios.h"
#include "dos_inc.h"
#include "support.h"
#include "setup.h"
#include "control.h"

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

/* ints/bios.cpp */
void bios_enable_ps2();

/* hardware/keyboard.cpp */
void AUX_INT33_Takeover();
int KEYBOARD_AUX_Active();
void KEYBOARD_AUX_Event(float x,float y,Bitu buttons,int scrollwheel);

bool en_int33=false;
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

static Bit16u ps2cbseg,ps2cbofs;
static bool useps2callback,ps2callbackinit;
static RealPt ps2_callback,uir_callback;
static Bit16s oldmouseX, oldmouseY;

// serial mouse emulation
void on_mouse_event_for_serial(int delta_x,int delta_y,Bit8u buttonstate);

struct button_event {
    Bit8u type;
    Bit8u buttons;
};

extern bool enable_slave_pic;
extern uint8_t p7fd8_8255_mouse_int_enable;

uint8_t MOUSE_IRQ = 12; // IBM PC/AT default

#define QUEUE_SIZE 32
#define MOUSE_BUTTONS 3
#define POS_X (static_cast<Bit16s>(mouse.x) & mouse.gran_x)
#define POS_Y (static_cast<Bit16s>(mouse.y) & mouse.gran_y)

#define CURSORX 16
#define CURSORY 16
#define HIGHESTBIT (1<<(CURSORX-1))

static Bit16u defaultTextAndMask = 0x77FF;
static Bit16u defaultTextXorMask = 0x7700;

static Bit16u defaultScreenMask[CURSORY] = {
        0x3FFF, 0x1FFF, 0x0FFF, 0x07FF,
        0x03FF, 0x01FF, 0x00FF, 0x007F,
        0x003F, 0x001F, 0x01FF, 0x00FF,
        0x30FF, 0xF87F, 0xF87F, 0xFCFF
};

static Bit16u defaultCursorMask[CURSORY] = {
        0x0000, 0x4000, 0x6000, 0x7000,
        0x7800, 0x7C00, 0x7E00, 0x7F00,
        0x7F80, 0x7C00, 0x6C00, 0x4600,
        0x0600, 0x0300, 0x0300, 0x0000
};

static Bit16u userdefScreenMask[CURSORY];
static Bit16u userdefCursorMask[CURSORY];

static struct {
    Bit8u buttons;
    Bit16u times_pressed[MOUSE_BUTTONS];
    Bit16u times_released[MOUSE_BUTTONS];
    Bit16u last_released_x[MOUSE_BUTTONS];
    Bit16u last_released_y[MOUSE_BUTTONS];
    Bit16u last_pressed_x[MOUSE_BUTTONS];
    Bit16u last_pressed_y[MOUSE_BUTTONS];
    pic_tickindex_t hidden_at;
    Bit16u hidden;
    float add_x,add_y;
    Bit16s min_x,max_x,min_y,max_y;
    Bit16s max_screen_x,max_screen_y;
    float mickey_x,mickey_y;
    float x,y;
    float ps2x,ps2y;
    button_event event_queue[QUEUE_SIZE];
    Bit8u events;//Increase if QUEUE_SIZE >255 (currently 32)
    Bit16u sub_seg,sub_ofs;
    Bit16u sub_mask;

    bool    background;
    Bit16s  backposx, backposy;
    Bit8u   backData[CURSORX*CURSORY];
    Bit16u* screenMask;
    Bit16u* cursorMask;
    Bit16s  clipx,clipy;
    Bit16s  hotx,hoty;
    Bit16u  textAndMask, textXorMask;

    float   mickeysPerPixel_x;
    float   mickeysPerPixel_y;
    float   pixelPerMickey_x;
    float   pixelPerMickey_y;
    Bit16u  senv_x_val;
    Bit16u  senv_y_val;
    Bit16u  dspeed_val;
    float   senv_x;
    float   senv_y;
    Bit16s  updateRegion_x[2];
    Bit16s  updateRegion_y[2];
    Bit16u  doubleSpeedThreshold;
    Bit16u  language;
    Bit16u  cursorType;
    Bit16u  oldhidden;
    Bit8u  page;
    bool enabled;
    bool inhibit_draw;
    bool timer_in_progress;
    bool first_range_setx;
    bool first_range_sety;
    bool in_UIR;
    Bit8u mode;
    Bit16s gran_x,gran_y;
    int scrollwheel;
} mouse;

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

void Mouse_ChangePS2Callback(Bit16u pseg, Bit16u pofs) {
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

void DoPS2Callback(Bit16u data, Bit16s mouseX, Bit16s mouseY) {
    if (useps2callback && ps2cbseg != 0 && ps2cbofs != 0) {
        Bit16u mdat = (data & 0x03) | 0x08;
        Bit16s xdiff = mouseX-oldmouseX;
        Bit16s ydiff = oldmouseY-mouseY;
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
        CPU_Push16((Bit16u)mdat); 
        CPU_Push16((Bit16u)(xdiff % 256)); 
        CPU_Push16((Bit16u)(ydiff % 256)); 
        CPU_Push16((Bit16u)0); 
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
#define MOUSE_DUMMY 128
#define MOUSE_DELAY 5.0

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

INLINE void Mouse_AddEvent(Bit8u type) {
    if (mouse.events<QUEUE_SIZE) {
        if (mouse.events>0) {
            /* Skip duplicate events */
            if (type==MOUSE_HAS_MOVED) return;
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
extern void WriteChar(Bit16u col,Bit16u row,Bit8u page,Bit16u chr,Bit8u attr,bool useattr);
extern void ReadCharAttr(Bit16u col,Bit16u row,Bit8u page,Bit16u * result);

void RestoreCursorBackgroundText() {
    if (mouse.hidden || mouse.inhibit_draw) return;

    if (mouse.background) {
        WriteChar((Bit16u)mouse.backposx,(Bit16u)mouse.backposy,real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE),mouse.backData[0],mouse.backData[1],true);
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
    Bit8u page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);

    if (mouse.cursorType == 0) {
        Bit16u result;
        ReadCharAttr((Bit16u)mouse.backposx,(Bit16u)mouse.backposy,page,&result);
        mouse.backData[0]	= (Bit8u)(result & 0xFF);
        mouse.backData[1]	= (Bit8u)(result>>8);
        mouse.background	= true;
        // Write Cursor
        result = (result & mouse.textAndMask) ^ mouse.textXorMask;
        WriteChar((Bit16u)mouse.backposx,(Bit16u)mouse.backposy,page,(Bit8u)(result&0xFF),(Bit8u)(result>>8),true);
    } else {
        Bit16u address=page * real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE);
        address += (mouse.backposy * real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS) + mouse.backposx) * 2;
        address /= 2;
        Bit16u cr = real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
        IO_Write(cr     , 0xe);
        IO_Write((Bitu)cr + 1u, (address >> 8) & 0xff);
        IO_Write(cr     , 0xf);
        IO_Write((Bitu)cr + 1u, address & 0xff);
    }
}

// ***************************************************************************
// Mouse cursor - graphic mode
// ***************************************************************************

static Bit8u gfxReg3CE[9];
static Bit8u index3C4,gfxReg3C5;
void SaveVgaRegisters() {
    if (IS_VGA_ARCH) {
        for (Bit8u i=0; i<9; i++) {
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
        for (Bit8u i=0; i<9; i++) {
            IO_Write(0x3CE,i);
            IO_Write(0x3CF,gfxReg3CE[i]);
        }

        IO_Write(0x3C4,2);
        IO_Write(0x3C5,gfxReg3C5);
        IO_Write(0x3C4,index3C4);
    }
}

void ClipCursorArea(Bit16s& x1, Bit16s& x2, Bit16s& y1, Bit16s& y2,
                    Bit16u& addx1, Bit16u& addx2, Bit16u& addy) {
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
        Bit16s x,y;
        Bit16u addx1,addx2,addy;
        Bit16u dataPos  = 0;
        Bit16s x1       = mouse.backposx;
        Bit16s y1       = mouse.backposy;
        Bit16s x2       = x1 + CURSORX - 1;
        Bit16s y2       = y1 + CURSORY - 1; 

        ClipCursorArea(x1, x2, y1, y2, addx1, addx2, addy);

        dataPos = addy * CURSORX;
        for (y=y1; y<=y2; y++) {
            dataPos += addx1;
            for (x=x1; x<=x2; x++) {
                INT10_PutPixel((Bit16u)x,(Bit16u)y,mouse.page,mouse.backData[dataPos++]);
            }
            dataPos += addx2;
        }
        mouse.background = false;
    }
    RestoreVgaRegisters();
}

void DrawCursor() {
    if (mouse.hidden || mouse.inhibit_draw) return;
    INT10_SetCurMode();
    // In Textmode ?
    if (CurMode->type==M_TEXT) {
        DrawCursorText();
        return;
    }

    // Check video page. Seems to be ignored for text mode. 
    // hence the text mode handled above this
    if (real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE)!=mouse.page) return;
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


    mouse.clipx = (Bit16s)((Bits)CurMode->swidth-1);    /* Get from bios ? */
    mouse.clipy = (Bit16s)((Bits)CurMode->sheight-1);

    /* might be vidmode == 0x13?2:1 */
    Bit16s xratio = 640;
    if (CurMode->swidth>0) xratio/=(Bit16u)CurMode->swidth;
    if (xratio==0) xratio = 1;
    
    RestoreCursorBackground();

    SaveVgaRegisters();

    // Save Background
    Bit16s x,y;
    Bit16u addx1,addx2,addy;
    Bit16u dataPos  = 0;
    Bit16s x1       = POS_X / xratio - mouse.hotx;
    Bit16s y1       = POS_Y - mouse.hoty;
    Bit16s x2       = x1 + CURSORX - 1;
    Bit16s y2       = y1 + CURSORY - 1; 

    ClipCursorArea(x1,x2,y1,y2, addx1, addx2, addy);

    dataPos = addy * CURSORX;
    for (y=y1; y<=y2; y++) {
        dataPos += addx1;
        for (x=x1; x<=x2; x++) {
            INT10_GetPixel((Bit16u)x,(Bit16u)y,mouse.page,&mouse.backData[dataPos++]);
        }
        dataPos += addx2;
    }
    mouse.background= true;
    mouse.backposx  = POS_X / xratio - mouse.hotx;
    mouse.backposy  = POS_Y - mouse.hoty;

    // Draw Mousecursor
    dataPos = addy * CURSORX;
    for (y=y1; y<=y2; y++) {
        Bit16u scMask = mouse.screenMask[addy+y-y1];
        Bit16u cuMask = mouse.cursorMask[addy+y-y1];
        if (addx1>0) { scMask<<=addx1; cuMask<<=addx1; dataPos += addx1; }
        for (x=x1; x<=x2; x++) {
            Bit8u pixel = 0;
            // ScreenMask
            if (scMask & HIGHESTBIT) pixel = mouse.backData[dataPos];
            scMask<<=1;
            // CursorMask
            if (cuMask & HIGHESTBIT) pixel = pixel ^ 0x0F;
            cuMask<<=1;
            // Set Pixel
            INT10_PutPixel((Bit16u)x,(Bit16u)y,mouse.page,pixel);
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

extern int  user_cursor_x,  user_cursor_y;
extern int  user_cursor_sw, user_cursor_sh;
extern bool user_cursor_locked;

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
    } else if (CurMode != NULL) {
        if (CurMode->type == M_TEXT) {
            mouse.x = x*real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8;
            mouse.y = y*(real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1)*8;
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

#if defined(WIN32)
char text[50*81];
const char* Mouse_GetSelected(int x1, int y1, int x2, int y2, int w, int h) {
	Bit8u page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
	Bit16u c=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS), r=(Bit16u)real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1;
	int c1=c*x1/w, r1=r*y1/h, c2=c*x2/w, r2=r*y2/h, t;
	char str[2];
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
	Bit16u result;
	text[0]=0;
	for (int i=r1; i<=r2; i++) {
		for (int j=c1; j<=c2; j++) {
			ReadCharAttr(j,i,page,&result);
			sprintf(str,"%c",result);
			if (str[0]==0) continue;
			strcat(text,str);
		}
		while (strlen(text)>0&&text[strlen(text)-1]==32) text[strlen(text)-1]=0;
		if (i<r2) strcat(text,"\r\n");
	}
	return text;
}

void Mouse_Select(int x1, int y1, int x2, int y2, int w, int h) {
	Bit8u page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
	Bit16u c=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS), r=(Bit16u)real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1;
	int c1=c*x1/w, r1=r*y1/h, c2=c*x2/w, r2=r*y2/h, t;
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
	for (int i=r1; i<=r2; i++)
		for (int j=c1; j<=c2; j++)
			real_writeb(0xb800,(i*c+j)*2+1,real_readb(0xb800,(i*c+j)*2+1)^119);
}

void Restore_Text(int x1, int y1, int x2, int y2, int w, int h) {
	Bit8u page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
	Bit16u c=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS), r=(Bit16u)real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1;
	int c1=c*x1/w, r1=r*y1/h, c2=c*x2/w, r2=r*y2/h, t;
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
	for (int i=r1; i<=r2; i++)
		for (int j=c1; j<=c2; j++)
			real_writeb(0xb800,(i*c+j)*2+1,real_readb(0xb800,(i*c+j)*2+1)^119);
}
#endif

void Mouse_ButtonPressed(Bit8u button) {
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
    mouse.last_pressed_x[button]=(Bit16u)POS_X;
    mouse.last_pressed_y[button]=(Bit16u)POS_Y;

    /* serial mouse, if connected, also wants to know about it */
    on_mouse_event_for_serial(0,0,mouse.buttons);
}

void Mouse_ButtonReleased(Bit8u button) {
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
    mouse.last_released_x[button]=(Bit16u)POS_X;
    mouse.last_released_y[button]=(Bit16u)POS_Y;

    /* serial mouse, if connected, also wants to know about it */
    on_mouse_event_for_serial(0,0,mouse.buttons);
}

static void Mouse_SetMickeyPixelRate(Bit16s px, Bit16s py){
    if ((px!=0) && (py!=0)) {
        mouse.mickeysPerPixel_x  = (float)px/X_MICKEY;
        mouse.mickeysPerPixel_y  = (float)py/Y_MICKEY;
        mouse.pixelPerMickey_x   = X_MICKEY/(float)px;
        mouse.pixelPerMickey_y   = Y_MICKEY/(float)py;  
    }
}

static void Mouse_SetSensitivity(Bit16u px, Bit16u py, Bit16u dspeed){
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
    if (!mouse.hidden) {
        mouse.hidden = 1;
        mouse.hidden_at = PIC_FullIndex();
    }
    mouse.oldhidden = 1;
    mouse.background = false;
}

//Does way to much. Many things should be moved to mouse reset one day
void Mouse_AfterNewVideoMode(bool setmode) {
    mouse.inhibit_draw = false;
    /* Get the correct resolution from the current video mode */
    Bit8u mode = mem_readb(BIOS_VIDEO_MODE);
    if (setmode && mode == mouse.mode) LOG(LOG_MOUSE,LOG_NORMAL)("New video mode is the same as the old");
    mouse.first_range_setx = false;
    mouse.first_range_sety = false;
    mouse.gran_x = (Bit16s)0xffff;
    mouse.gran_y = (Bit16s)0xffff;

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
        mouse.gran_y = (Bit16s)0xfff8;
        if (IS_PC98_ARCH) {
            mouse.max_y = 400 - 1;
        }
        else {
            Bitu rows = real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS);
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
        if (mode == 0x0d || mode == 0x13) mouse.gran_x = (Bit16s)0xfffe;
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
        mouse.gran_x = (Bit16s)0xffff;
        mouse.gran_y = (Bit16s)0xffff;
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

    for (Bit16u but=0; but<MOUSE_BUTTONS; but++) {
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

static Bitu INT33_Handler(void) {
//  LOG(LOG_MOUSE,LOG_NORMAL)("MOUSE: %04X %X %X %d %d",reg_ax,reg_bx,reg_cx,POS_X,POS_Y);
    switch (reg_ax) {
    case 0x00:  /* Reset Driver and Read Status */
        Mouse_ResetHardware();
        goto software_reset;
    case 0x01:  /* Show Mouse */
        if (mouse.hidden) mouse.hidden--;
        mouse.updateRegion_y[1] = -1; //offscreen
        Mouse_AutoLock(true);
        DrawCursor();
        break;
    case 0x02:  /* Hide Mouse */
        {
            if (CurMode->type != M_TEXT) RestoreCursorBackground();
            else RestoreCursorBackgroundText();
            if (mouse.hidden == 0) mouse.hidden_at = PIC_FullIndex();
            mouse.hidden++;
        }
        break;
    case 0x03:  /* Return position and Button Status */
        reg_bx = mouse.buttons;
        reg_cx = (Bit16u)POS_X;
        reg_dx = (Bit16u)POS_Y;
        mouse.first_range_setx = false;
        mouse.first_range_sety = false;
        if (en_int33_hide_if_polling) int33_last_poll = PIC_FullIndex();
        break;
    case 0x04:  /* Position Mouse */
        /* If position isn't different from current position
         * don't change it then. (as position is rounded so numbers get
         * lost when the rounded number is set) (arena/simulation Wolf) */
        if ((Bit16s)reg_cx >= mouse.max_x) mouse.x = static_cast<float>(mouse.max_x);
        else if (mouse.min_x >= (Bit16s)reg_cx) mouse.x = static_cast<float>(mouse.min_x);
        else if ((Bit16s)reg_cx != POS_X) mouse.x = static_cast<float>(reg_cx);

        if ((Bit16s)reg_dx >= mouse.max_y) mouse.y = static_cast<float>(mouse.max_y);
        else if (mouse.min_y >= (Bit16s)reg_dx) mouse.y = static_cast<float>(mouse.min_y);
        else if ((Bit16s)reg_dx != POS_Y) mouse.y = static_cast<float>(reg_dx);
        DrawCursor();
        if (en_int33_hide_if_polling) int33_last_poll = PIC_FullIndex();
        break;
    case 0x05:  /* Return Button Press Data */
        {
            Bit16u but = reg_bx;
            reg_ax = mouse.buttons;
            if (but >= MOUSE_BUTTONS) but = MOUSE_BUTTONS - 1;
            reg_cx = mouse.last_pressed_x[but];
            reg_dx = mouse.last_pressed_y[but];
            reg_bx = mouse.times_pressed[but];
            mouse.times_pressed[but] = 0;
            if (en_int33_hide_if_polling) int33_last_poll = PIC_FullIndex();
            break;
        }
    case 0x06:  /* Return Button Release Data */
        {
            Bit16u but = reg_bx;
            reg_ax = mouse.buttons;
            if (but >= MOUSE_BUTTONS) but = MOUSE_BUTTONS - 1;
            reg_cx = mouse.last_released_x[but];
            reg_dx = mouse.last_released_y[but];
            reg_bx = mouse.times_released[but];
            mouse.times_released[but] = 0;
            if (en_int33_hide_if_polling) int33_last_poll = PIC_FullIndex();
            break;
        }
    case 0x07:  /* Define horizontal cursor range */
        {
            //Lemmings sets 1-640 and wants that. Ironseed sets 0-640 but doesn't like 640
            //Ironseed works if newvideo mode with mode 13 sets 0-639
            //Larry 6 actually wants newvideo mode with mode 13 to set it to 0-319
            Bit16s max, min;
            if ((Bit16s)reg_cx < (Bit16s)reg_dx) { min = (Bit16s)reg_cx; max = (Bit16s)reg_dx; }
            else { min = (Bit16s)reg_dx; max = (Bit16s)reg_cx; }
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
                    Bit16s nval = mouse.max_x;

                    if (CurMode->type == M_TEXT) {
                        // Text is reported as if each row is 8 lines high (CGA compat) even if EGA 14-line
                        // or VGA 16-line, and 8 pixels wide even if EGA/VGA 9-pixels/char is enabled.
                        //
                        // Apply sanity rounding.
                        //
                        // FreeDOS EDIT: The max is set to just under 640x400, so that the cursor only has
                        //               room for ONE PIXEL in the last row and column.
                        if (nval >= ((Bit16s)(CurMode->twidth*8) - 32) && nval <= ((Bit16s)(CurMode->twidth*8) + 32))
                            nval = (Bit16s)CurMode->twidth*8;
                    }
                    else {
                        // Apply sanity rounding.
                        //
                        // Daggerfall: Sets max to 310 instead of 320, probably to prevent drawing the cursor
                        //             partially offscreen. */
                        if (nval >= ((Bit16s)CurMode->swidth - 32) && nval <= ((Bit16s)CurMode->swidth + 32))
                            nval = (Bit16s)CurMode->swidth;
                        else if (nval >= (((Bit16s)CurMode->swidth - 32) * 2) && nval <= (((Bit16s)CurMode->swidth + 32) * 2))
                            nval = (Bit16s)CurMode->swidth * 2;
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
    case 0x08:  /* Define vertical cursor range */
        {
            // Not sure what to take instead of the CurMode (see case 0x07 as well)
            // especially the cases where sheight= 400 and we set it with the mouse_reset to 200
            // disabled it at the moment. Seems to break Syndicate which wants 400 in mode 13
            Bit16s max, min;
            if ((Bit16s)reg_cx < (Bit16s)reg_dx) { min = (Bit16s)reg_cx; max = (Bit16s)reg_dx; }
            else { min = (Bit16s)reg_dx; max = (Bit16s)reg_cx; }
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
                    Bit16s nval = mouse.max_y;

                    if (CurMode->type == M_TEXT) {
                        // Text is reported as if each row is 8 lines high (CGA compat) even if EGA 14-line
                        // or VGA 16-line, and 8 pixels wide even if EGA/VGA 9-pixels/char is enabled.
                        //
                        // Apply sanity rounding.
                        //
                        // FreeDOS EDIT: The max is set to just under 640x400, so that the cursor only has
                        //               room for ONE PIXEL in the last row and column.
                        if (nval >= ((Bit16s)(CurMode->theight*8) - 32) && nval <= ((Bit16s)(CurMode->theight*8) + 32))
                            nval = (Bit16s)CurMode->theight*8;
                    }
                    else {
                        // Apply sanity rounding.
                        //
                        // Daggerfall: Sets max to 310 instead of 320, probably to prevent drawing the cursor
                        //             partially offscreen. */
                        if (nval >= ((Bit16s)CurMode->sheight - 32) && nval <= ((Bit16s)CurMode->sheight + 32))
                            nval = (Bit16s)CurMode->sheight;
                        else if (nval >= (((Bit16s)CurMode->sheight - 32) * 2) && nval <= (((Bit16s)CurMode->sheight + 32) * 2))
                            nval = (Bit16s)CurMode->sheight * 2;
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
    case 0x09:  /* Define GFX Cursor */
        {
            PhysPt src = SegPhys(es) + reg_dx;
            MEM_BlockRead(src, userdefScreenMask, CURSORY * 2);
            MEM_BlockRead(src + CURSORY * 2, userdefCursorMask, CURSORY * 2);
            mouse.screenMask = userdefScreenMask;
            mouse.cursorMask = userdefCursorMask;
            mouse.hotx = (Bit16s)reg_bx;
            mouse.hoty = (Bit16s)reg_cx;
            mouse.cursorType = 2;
            DrawCursor();
            break;
        }
    case 0x0a:  /* Define Text Cursor */
        mouse.cursorType = (reg_bx ? 1 : 0);
        mouse.textAndMask = reg_cx;
        mouse.textXorMask = reg_dx;
        if (reg_bx) {
            INT10_SetCursorShape(reg_cl, reg_dl);
            LOG(LOG_MOUSE, LOG_NORMAL)("Hardware Text cursor selected");
        }
        DrawCursor();
        break;
    case 0x0b:  /* Read Motion Data */
        {
            extern bool MOUSE_IsLocked();
            const auto locked = MOUSE_IsLocked();
            reg_cx = (Bit16u)static_cast<Bit16s>(locked ? mouse.mickey_x : 0);
            reg_dx = (Bit16u)static_cast<Bit16s>(locked ? mouse.mickey_y : 0);
            mouse.mickey_x = 0;
            mouse.mickey_y = 0;
            break;
        }
    case 0x0c:  /* Define interrupt subroutine parameters */
        mouse.sub_mask = reg_cx;
        mouse.sub_seg = SegValue(es);
        mouse.sub_ofs = reg_dx;
        Mouse_AutoLock(true); //Some games don't seem to reset the mouse before using
        break;
    case 0x0d:  /* Mouse light pen emulation on */
        LOG(LOG_MOUSE, LOG_ERROR)("Mouse light pen emulation on not implemented");
        break;
    case 0x0e:  /* Mouse light pen emulation off */
        LOG(LOG_MOUSE, LOG_ERROR)("Mouse light pen emulation off not implemented");
        break;
    case 0x0f:  /* Define mickey/pixel rate */
        Mouse_SetMickeyPixelRate((Bit16s)reg_cx, (Bit16s)reg_dx);
        break;
    case 0x10:  /* Define screen region for updating */
        mouse.updateRegion_x[0] = (Bit16s)reg_cx;
        mouse.updateRegion_y[0] = (Bit16s)reg_dx;
        mouse.updateRegion_x[1] = (Bit16s)reg_si;
        mouse.updateRegion_y[1] = (Bit16s)reg_di;
        DrawCursor();
        break;
    case 0x11:      /* Get number of buttons */
        reg_ax = 0xffff;
        reg_bx = MOUSE_BUTTONS;
        break;
    case 0x13:      /* Set double-speed threshold */
        mouse.doubleSpeedThreshold = (reg_bx ? reg_bx : 64);
        break;
    case 0x14: /* Exchange event-handler */
        {
            Bit16u oldSeg = mouse.sub_seg;
            Bit16u oldOfs = mouse.sub_ofs;
            Bit16u oldMask = mouse.sub_mask;
            // Set new values
            mouse.sub_mask = reg_cx;
            mouse.sub_seg = SegValue(es);
            mouse.sub_ofs = reg_dx;
            // Return old values
            reg_cx = (Bit16u)oldMask;
            reg_dx = (Bit16u)oldOfs;
            SegSet16(es, oldSeg);
        }
        break;
    case 0x15: /* Get Driver storage space requirements */
        reg_bx = sizeof(mouse);
        break;
    case 0x16: /* Save driver state */
        {
            LOG(LOG_MOUSE, LOG_NORMAL)("Saving driver state...");
            PhysPt dest = SegPhys(es) + reg_dx;
            MEM_BlockWrite(dest, &mouse, sizeof(mouse));
        }
        break;
    case 0x17: /* load driver state */
        {
            LOG(LOG_MOUSE, LOG_NORMAL)("Loading driver state...");
            PhysPt src = SegPhys(es) + reg_dx;
            MEM_BlockRead(src, &mouse, sizeof(mouse));
            break;
        }
    case 0x18: /* Set alternate subroutine call mask and address */
        LOG(LOG_MOUSE, LOG_ERROR)("Set alternate subroutine call mask and address not implemented");
        break;
    case 0x19: /* Get user alternate interrupt address*/
        LOG(LOG_MOUSE, LOG_ERROR)("Get user alternate interrupt address not implemented");
        break;
    case 0x1a:  /* Set mouse sensitivity */
        // ToDo : double mouse speed value
        Mouse_SetSensitivity(reg_bx, reg_cx, reg_dx);
        LOG(LOG_MOUSE, LOG_NORMAL)("Set sensitivity used with %d %d (%d)", reg_bx, reg_cx, reg_dx);
        break;
    case 0x1b:  /* Get mouse sensitivity */
        reg_bx = mouse.senv_x_val;
        reg_cx = mouse.senv_y_val;
        reg_dx = mouse.dspeed_val;

        LOG(LOG_MOUSE, LOG_NORMAL)("Get sensitivity %d %d", reg_bx, reg_cx);
        break;
    case 0x1c:  /* Set interrupt rate */
        /* Can't really set a rate this is host determined */
        break;
    case 0x1d:      /* Set display page number */
        mouse.page = reg_bl;
        break;
    case 0x1e:      /* Get display page number */
        reg_bx = mouse.page;
        break;
    case 0x1f:  /* Disable Mousedriver */
        /* ES:BX old mouse driver Zero at the moment TODO */
        reg_bx = 0;
        SegSet16(es, 0);
        mouse.enabled = false; /* Just for reporting not doing a thing with it */
        mouse.oldhidden = mouse.hidden;
        if (!mouse.hidden) {
            mouse.hidden = 1;
            mouse.hidden_at = PIC_FullIndex();
        }
        break;
    case 0x20:  /* Enable Mousedriver */
        mouse.enabled = true;
        mouse.hidden = mouse.oldhidden;
        break;
    case 0x21:  /* Software Reset */
    software_reset:
        extern bool Mouse_Drv;
        if (Mouse_Drv) {
            reg_ax = 0xffff;
            reg_bx = MOUSE_BUTTONS;
            Mouse_Reset();
            Mouse_AutoLock(true);
            AUX_INT33_Takeover();
            LOG(LOG_MOUSE, LOG_NORMAL)("INT 33h reset");
        }
        break;
    case 0x22:      /* Set language for messages */
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
             *                        07h     Portugese
             *                        08h     Italian
             *
             */
        mouse.language = reg_bx;
        break;
    case 0x23:      /* Get language for messages */
        reg_bx = mouse.language;
        break;
    case 0x24:  /* Get Software version and mouse type */
        reg_bx = 0x805;   //Version 8.05 woohoo 
        reg_ch = 0x04;    /* PS/2 type */
        reg_cl = 0;       /* PS/2 (unused) */
        break;
    case 0x26: /* Get Maximum virtual coordinates */
        reg_bx = (mouse.enabled ? 0x0000 : 0xffff);
        reg_cx = (Bit16u)mouse.max_x;
        reg_dx = (Bit16u)mouse.max_y;
        break;
    case 0x2a:  /* Get cursor hot spot */
        reg_al = (Bit8u)-mouse.hidden;    // Microsoft uses a negative byte counter for cursor visibility
        reg_bx = (Bit16u)mouse.hotx;
        reg_cx = (Bit16u)mouse.hoty;
        reg_dx = 0x04;    // PS/2 mouse type
        break;
    case 0x31: /* Get Current Minimum/Maximum virtual coordinates */
        reg_ax = (Bit16u)mouse.min_x;
        reg_bx = (Bit16u)mouse.min_y;
        reg_cx = (Bit16u)mouse.max_x;
        reg_dx = (Bit16u)mouse.max_y;
        break;
    case 0x53C1: /* Logitech CyberMan */
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
    Bit16u raxpt=real_readw(SegValue(ss),reg_sp+0x0a);
    Bit16u rbxpt=real_readw(SegValue(ss),reg_sp+0x08);
    Bit16u rcxpt=real_readw(SegValue(ss),reg_sp+0x06);
    Bit16u rdxpt=real_readw(SegValue(ss),reg_sp+0x04);

    // read out the actual values, registers ARE overwritten
    Bit16u rax=real_readw(SegValue(ds),raxpt);
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
        if (en_int33) DrawCursor();

        /* Check for an active Interrupt Handler that will get called */
        if (mouse.sub_mask & mouse.event_queue[mouse.events].type) {
            reg_ax=mouse.event_queue[mouse.events].type;
            reg_bx=mouse.event_queue[mouse.events].buttons;
            reg_cx=(Bit16u)POS_X;
            reg_dx=(Bit16u)POS_Y;
            reg_si=(Bit16u)static_cast<Bit16s>(mouse.mickey_x);
            reg_di=(Bit16u)static_cast<Bit16s>(mouse.mickey_y);
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
            DoPS2Callback(mouse.event_queue[mouse.events].buttons, static_cast<Bit16s>(mouse.ps2x), static_cast<Bit16s>(mouse.ps2y));
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
        Bit8u hwvec=(MOUSE_IRQ>7)?(0x70+MOUSE_IRQ-8):(0x8+MOUSE_IRQ);
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
    RealPt i33loc=0;

    /* TODO: Needs to check for mouse, and fail to do anything if neither PS/2 nor serial mouse emulation enabled */

    en_int33_hide_if_intsub=section->Get_bool("int33 hide host cursor if interrupt subroutine");

    en_int33_hide_if_polling=section->Get_bool("int33 hide host cursor when polling");

    en_int33_pc98_show_graphics=section->Get_bool("pc-98 show graphics layer on initialize");

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

    if (!mouse.hidden) {
        mouse.hidden = 1;
        mouse.hidden_at = PIC_FullIndex();
    }

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

