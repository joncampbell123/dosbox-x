/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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


#include "dosbox.h"
#include "keyboard.h"
#include "support.h"
#include "setup.h"
#include "inout.h"
#include "mouse.h"
#include "menu.h"
#include "pic.h"
#include "mem.h"
#include "cpu.h"
#include "mixer.h"
#include "timer.h"
#include <math.h>
#include "8255.h"

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

#define KEYBUFSIZE 32*3
#define RESETDELAY 400
#define KEYDELAY 0.300f         //Considering 20-30 khz serial clock and 11 bits/char

#define AUX 0x100

void AUX_Reset();
void KEYBOARD_Reset();
static void KEYBOARD_SetPort60(uint16_t val);
void KEYBOARD_AddBuffer(uint16_t data);
static void KEYBOARD_Add8042Response(uint8_t data);
void KEYBOARD_SetLEDs(uint8_t bits);

bool enable_pc98_bus_mouse = true;

static unsigned int aux_warning = 0;

enum AuxCommands {
    ACMD_NONE,
    ACMD_SET_RATE,
    ACMD_SET_RESOLUTION
};

enum KeyCommands {
    CMD_NONE,
    CMD_SETLEDS,
    CMD_SETTYPERATE,
    CMD_SETOUTPORT,
    CMD_SETCOMMAND,
    CMD_WRITEOUTPUT,
    CMD_WRITEAUXOUT,
    CMD_SETSCANSET,
    CMD_WRITEAUX
};

enum MouseMode {
    MM_REMOTE=0,
    MM_WRAP,
    MM_STREAM
};

enum MouseType {
    MOUSE_NONE=0,
    MOUSE_2BUTTON,
    MOUSE_3BUTTON,
    MOUSE_INTELLIMOUSE,
    MOUSE_INTELLIMOUSE45
};

struct ps2mouse {
    MouseType   type;           /* what kind of mouse we are emulating */
    MouseMode   mode;           /* current mode */
    MouseMode   reset_mode;     /* mode to change to on reset */
    uint8_t       samplerate;     /* current sample rate */
    uint8_t       resolution;     /* current resolution */
    uint8_t       last_srate[3];      /* last 3 "set sample rate" values */
    float       acx,acy;        /* accumulator */
    bool        reporting;      /* reporting */
    bool        scale21;        /* 2:1 scaling */
    bool        intellimouse_mode;  /* intellimouse scroll wheel */
    bool        intellimouse_btn45; /* 4th & 5th buttons */
    bool        int33_taken;        /* for compatability with existing DOSBox code: allow INT 33H emulation to "take over" and disable us */
    bool        l,m,r;          /* mouse button states */
};

static struct {
    uint8_t buf8042[8];       /* for 8042 responses, taking priority over keyboard responses */
    Bitu buf8042_len;
    Bitu buf8042_pos;
    int pending_key;

    uint16_t buffer[KEYBUFSIZE];
    Bitu used;
    Bitu pos;

    struct {
        KBD_KEYS key;
        Bitu wait;
        Bitu pause,rate;
    } repeat;
    struct ps2mouse ps2mouse;
    KeyCommands command;
    AuxCommands aux_command;
    Bitu led_state;
    uint8_t p60data;
    uint8_t scanset;
    bool enable_aux;
    bool reset;
    bool active;
    bool scanning;
    bool auxactive;
    bool scheduled;
    bool p60changed;
    bool auxchanged;
    bool pending_key_state;
    /* command byte related */
    bool cb_override_inhibit;
    bool cb_irq12;          /* PS/2 mouse */
    bool cb_irq1;
    bool cb_xlat;
    bool cb_sys;
    bool leftctrl_pressed;
    bool rightctrl_pressed;
} keyb;

void PCjr_stuff_scancode(const unsigned char c) {
    keyb.p60data = c;
}

uint8_t Mouse_GetButtonState(void);

uint32_t Keyb_ig_status() {
    uint8_t mousebtn = Mouse_GetButtonState() & 7;

    return  ((uint32_t)keyb.led_state     << (uint32_t)0 ) |
        ((uint32_t)keyb.scanset       << (uint32_t)8 ) |
        ((uint32_t)keyb.reset         << (uint32_t)10) |
        ((uint32_t)keyb.active        << (uint32_t)11) |
        ((uint32_t)keyb.scanning      << (uint32_t)12) |
        ((uint32_t)keyb.auxactive     << (uint32_t)13) |
        ((uint32_t)keyb.scheduled     << (uint32_t)14) |
        ((uint32_t)keyb.p60changed    << (uint32_t)15) |
        ((uint32_t)keyb.auxchanged    << (uint32_t)16) |
        ((uint32_t)keyb.cb_xlat       << (uint32_t)17) |
        ((uint32_t)keyb.ps2mouse.l    << (uint32_t)18) |
        ((uint32_t)keyb.ps2mouse.m    << (uint32_t)19) |
        ((uint32_t)keyb.ps2mouse.r    << (uint32_t)20) |
        ((uint32_t)keyb.ps2mouse.reporting << (uint32_t)21) |
        ((uint32_t)(keyb.ps2mouse.mode == MM_STREAM ? 1 : 0) << (uint32_t)22) |
        ((uint32_t)mousebtn           << (uint32_t)23);
}

bool MouseTypeNone() {
    return (keyb.ps2mouse.type == MOUSE_NONE);
}

/* NTS: INT33H emulation is coded to call this ONLY if it hasn't taken over the role of mouse input */
void KEYBOARD_AUX_Event(float x,float y,Bitu buttons,int scrollwheel) {
    if (IS_PC98_ARCH) {
        LOG_MSG("WARNING: KEYBOARD_AUX_Event called in PC-98 emulation mode. This is a bug.");
        return;
    }

    keyb.ps2mouse.acx += x;
    keyb.ps2mouse.acy += y;
    keyb.ps2mouse.l = (buttons & 1)>0;
    keyb.ps2mouse.r = (buttons & 2)>0;
    keyb.ps2mouse.m = (buttons & 4)>0;

    /* "Valid ranges are -8 to 7"
     * http://www.computer-engineering.org/ps2mouse/ */
    if (scrollwheel < -8)
        scrollwheel = -8;
    else if (scrollwheel > 7)
        scrollwheel = 7;

    if (keyb.ps2mouse.reporting && keyb.ps2mouse.mode == MM_STREAM) {
        if ((keyb.used+4) < KEYBUFSIZE) {
            int x2,y2;

            x2 = (int)(keyb.ps2mouse.acx * (1 << keyb.ps2mouse.resolution));
            x2 /= 16; /* FIXME: Or else the cursor is WAY too sensitive in Windows 3.1 */
            if (x2 < -256) x2 = -256;
            else if (x2 > 255) x2 = 255;

            y2 = -((int)(keyb.ps2mouse.acy * (1 << keyb.ps2mouse.resolution)));
            y2 /= 16; /* FIXME: Or else the cursor is WAY too sensitive in Windows 3.1 */
            if (y2 < -256) y2 = -256;
            else if (y2 > 255) y2 = 255;

            KEYBOARD_AddBuffer(AUX|
                ((y2 == -256 || y2 == 255) ? 0x80 : 0x00) |   /* Y overflow */
                ((x2 == -256 || x2 == 255) ? 0x40 : 0x00) |   /* X overflow */
                ((y2 & 0x100) ? 0x20 : 0x00) |         /* Y sign bit */
                ((x2 & 0x100) ? 0x10 : 0x00) |         /* X sign bit */
                0x08 |                      /* always 1? */
                (keyb.ps2mouse.m ? 4 : 0) |         /* M */
                (keyb.ps2mouse.r ? 2 : 0) |         /* R */
                (keyb.ps2mouse.l ? 1 : 0));         /* L */
            KEYBOARD_AddBuffer(AUX|(x2&0xFF));
            KEYBOARD_AddBuffer(AUX|(y2&0xFF));
            if (keyb.ps2mouse.intellimouse_btn45) {
                KEYBOARD_AddBuffer(AUX|(scrollwheel&0xFF)); /* TODO: 4th & 5th buttons */
            }
            else if (keyb.ps2mouse.intellimouse_mode) {
                KEYBOARD_AddBuffer(AUX|(scrollwheel&0xFF));
            }
        }

        keyb.ps2mouse.acx = 0;
        keyb.ps2mouse.acy = 0;
    }
}

int KEYBOARD_AUX_Active() {
    /* NTS: We want to allow software to read by polling, which doesn't
     *      require interrupts to be enabled. Whether or not IRQ12 is
     *      unmasked is irrelevent */
    return keyb.auxactive && !keyb.ps2mouse.int33_taken;
}

static void KEYBOARD_SetPort60(uint16_t val) {
    keyb.auxchanged=(val&AUX)>0;
    keyb.p60changed=true;
    keyb.p60data=(uint8_t)val;
    if (keyb.auxchanged) {
        if (keyb.cb_irq12) {
            PIC_ActivateIRQ(12);
        }
    }
    else {
        if (keyb.cb_irq1) {
            if (machine == MCH_PCJR) CPU_Raise_NMI(); /* NTS: PCjr apparently hooked the keyboard to NMI */
            else PIC_ActivateIRQ(1);
        }
    }
}

static void KEYBOARD_ResetDelay(Bitu val) {
    (void)val;//UNUSED
    keyb.reset=false;
    KEYBOARD_SetLEDs(0);
    KEYBOARD_Add8042Response(0x00); /* BAT */
}

static void KEYBOARD_TransferBuffer(Bitu val) {
    (void)val;//UNUSED
    /* 8042 responses take priority over the keyboard */
    if (keyb.enable_aux && keyb.buf8042_len != 0) {
        KEYBOARD_SetPort60(keyb.buf8042[keyb.buf8042_pos]);
        if (++keyb.buf8042_pos >= keyb.buf8042_len)
            keyb.buf8042_len = keyb.buf8042_pos = 0;
        return;
    }

    keyb.scheduled=false;
    if (!keyb.used) {
        LOG(LOG_KEYBOARD,LOG_NORMAL)("Transfer started with empty buffer");
        return;
    }
    KEYBOARD_SetPort60(keyb.buffer[keyb.pos]);
    if (++keyb.pos>=KEYBUFSIZE) keyb.pos-=KEYBUFSIZE;
    keyb.used--;
}

void KEYBOARD_ClrBuffer(void) {
    keyb.buf8042_len=0;
    keyb.buf8042_pos=0;
    keyb.used=0;
    keyb.pos=0;
    PIC_RemoveEvents(KEYBOARD_TransferBuffer);
    keyb.scheduled=false;
}

size_t KEYBOARD_BufferSpaceAvail()   // emendelson from dbDOS
{
    return (KEYBUFSIZE - keyb.used);
}                                   // end emendelson from dbDOS

static void KEYBOARD_Add8042Response(uint8_t data) {
    if (keyb.buf8042_pos >= keyb.buf8042_len)
        keyb.buf8042_pos = keyb.buf8042_len = 0;
    else if (keyb.buf8042_len == 0)
        keyb.buf8042_pos = 0;

    if (keyb.buf8042_pos >= sizeof(keyb.buf8042)) {
        LOG(LOG_KEYBOARD,LOG_NORMAL)("8042 Buffer full, dropping code");
        KEYBOARD_ClrBuffer(); return;
    }

    keyb.buf8042[keyb.buf8042_len++] = data;
    PIC_AddEvent(KEYBOARD_TransferBuffer,KEYDELAY);
}

void KEYBOARD_AddBuffer(uint16_t data) {
    if (keyb.used>=KEYBUFSIZE) {
        LOG(LOG_KEYBOARD,LOG_NORMAL)("Buffer full, dropping code");
        KEYBOARD_ClrBuffer(); return;
    }
    Bitu start=keyb.pos+keyb.used;
    if (start>=KEYBUFSIZE) start-=KEYBUFSIZE;
    keyb.buffer[start]=data;
    keyb.used++;
    /* Start up an event to start the first IRQ */
    if (!keyb.scheduled && !keyb.p60changed) {
        keyb.scheduled=true;
        PIC_AddEvent(KEYBOARD_TransferBuffer,KEYDELAY);
    }
}

Bitu Keyboard_Guest_LED_State() {
    return keyb.led_state;
}

void UpdateKeyboardLEDState(Bitu led_state/* in the same bitfield arrangement as using command 0xED on PS/2 keyboards */);

void KEYBOARD_SetLEDs(uint8_t bits) {
    /* Some OSes we have control of the LEDs if keyboard+mouse capture */
    keyb.led_state = bits;
    UpdateKeyboardLEDState(bits);

    /* TODO: Maybe someday you could have DOSBox show the LEDs */

    /* log for debug info */
    LOG(LOG_KEYBOARD,LOG_DEBUG)("Keyboard LEDs: SCR=%u NUM=%u CAPS=%u",bits&1,(bits>>1)&1,(bits>>2)&1);
}

static Bitu read_p60(Bitu port,Bitu iolen) {
    (void)port;//UNUSED
    (void)iolen;//UNUSED
    keyb.p60changed=false;
    keyb.auxchanged=false;
    if (!keyb.scheduled && keyb.used) {
        keyb.scheduled=true;
        PIC_AddEvent(KEYBOARD_TransferBuffer,KEYDELAY);
    }
    return keyb.p60data;
}

unsigned char KEYBOARD_AUX_GetType() {
    /* and then the ID */
    if (keyb.ps2mouse.intellimouse_btn45)
        return 0x04;
    else if (keyb.ps2mouse.intellimouse_mode)
        return 0x03;
    else
        return 0x00;
}

unsigned char KEYBOARD_AUX_DevStatus() {
    return  (keyb.ps2mouse.mode == MM_REMOTE ? 0x40 : 0x00)|
        (keyb.ps2mouse.reporting << 5)|
        (keyb.ps2mouse.scale21 << 4)|
        (keyb.ps2mouse.m << 2)|
        (keyb.ps2mouse.r << 1)|
        (keyb.ps2mouse.l << 0);
}

unsigned char KEYBOARD_AUX_Resolution() {
    return keyb.ps2mouse.resolution;
}

unsigned char KEYBOARD_AUX_SampleRate() {
    return keyb.ps2mouse.samplerate;
}

void KEYBOARD_AUX_Write(Bitu val) {
    if (keyb.ps2mouse.type == MOUSE_NONE)
        return;

    if (keyb.ps2mouse.mode == MM_WRAP) {
        if (val != 0xFF && val != 0xEC) {
            KEYBOARD_AddBuffer(AUX|val);
            return;
        }
    }

    switch (keyb.aux_command) {
        case ACMD_NONE:
            switch (val) {
                case 0xff:  /* reset */
                    LOG(LOG_KEYBOARD,LOG_NORMAL)("AUX reset");
                    KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */
                    KEYBOARD_AddBuffer(AUX|0xaa);   /* reset */
                    KEYBOARD_AddBuffer(AUX|0x00);   /* i am mouse */
                    Mouse_AutoLock(false);
                    AUX_Reset();
                    break;
                case 0xf6:  /* set defaults */
                    KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */
                    AUX_Reset();
                    break;
                case 0xf5:  /* disable data reporting */
                    KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */
                    keyb.ps2mouse.reporting = false;
                    break;
                case 0xf4:  /* enable data reporting */
                    KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */
                    keyb.ps2mouse.reporting = true;
                    Mouse_AutoLock(true);
                    break;
                case 0xf3:  /* set sample rate */
                    KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */
                    keyb.aux_command = ACMD_SET_RATE;
                    break;
                case 0xf2:  /* get device ID */
                    KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */

                    /* and then the ID */
                    if (keyb.ps2mouse.intellimouse_btn45)
                        KEYBOARD_AddBuffer(AUX|0x04);
                    else if (keyb.ps2mouse.intellimouse_mode)
                        KEYBOARD_AddBuffer(AUX|0x03);
                    else
                        KEYBOARD_AddBuffer(AUX|0x00);
                    break;
                case 0xf0:  /* set remote mode */
                    KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */
                    keyb.ps2mouse.mode = MM_REMOTE;
                    break;
                case 0xee:  /* set wrap mode */
                    KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */
                    keyb.ps2mouse.mode = MM_WRAP;
                    break;
                case 0xec:  /* reset wrap mode */
                    KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */
                    keyb.ps2mouse.mode = MM_REMOTE;
                    break;
                case 0xeb:  /* read data */
                    KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */
                    KEYBOARD_AUX_Event(0,0,
                        ((unsigned int)keyb.ps2mouse.m << 2u) |
                        ((unsigned int)keyb.ps2mouse.r << 1u) |
                        ((unsigned int)keyb.ps2mouse.l << 0u),
                        0);
                    break;
                case 0xea:  /* set stream mode */
                    KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */
                    keyb.ps2mouse.mode = MM_STREAM;
                    break;
                case 0xe9:  /* status request */
                    KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */
                    KEYBOARD_AddBuffer(AUX|KEYBOARD_AUX_DevStatus());
                    KEYBOARD_AddBuffer(AUX|keyb.ps2mouse.resolution);
                    KEYBOARD_AddBuffer(AUX|keyb.ps2mouse.samplerate);
                    break;
                case 0xe8:  /* set resolution */
                    KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */
                    keyb.aux_command = ACMD_SET_RESOLUTION;
                    break;
                case 0xe7:  /* set scaling 2:1 */
                    KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */
                    keyb.ps2mouse.scale21 = true;
                    LOG(LOG_KEYBOARD,LOG_NORMAL)("PS/2 mouse scaling 2:1");
                    break;
                case 0xe6:  /* set scaling 1:1 */
                    KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */
                    keyb.ps2mouse.scale21 = false;
                    LOG(LOG_KEYBOARD,LOG_NORMAL)("PS/2 mouse scaling 1:1");
                    break;
            }
            break;
        case ACMD_SET_RATE:
            KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */
            memmove(keyb.ps2mouse.last_srate,keyb.ps2mouse.last_srate+1,2);
            keyb.ps2mouse.last_srate[2] = val;
            keyb.ps2mouse.samplerate = val;
            keyb.aux_command = ACMD_NONE;
            LOG(LOG_KEYBOARD,LOG_NORMAL)("PS/2 mouse sample rate set to %u",(int)val);
            if (keyb.ps2mouse.type >= MOUSE_INTELLIMOUSE) {
                if (keyb.ps2mouse.last_srate[0] == 200 && keyb.ps2mouse.last_srate[2] == 80) {
                    if (keyb.ps2mouse.last_srate[1] == 100) {
                        if (!keyb.ps2mouse.intellimouse_mode) {
                            LOG(LOG_KEYBOARD,LOG_NORMAL)("Intellimouse mode enabled");
                            keyb.ps2mouse.intellimouse_mode=true;
                        }
                    }
                    else if (keyb.ps2mouse.last_srate[1] == 200 && keyb.ps2mouse.type >= MOUSE_INTELLIMOUSE45) {
                        if (!keyb.ps2mouse.intellimouse_btn45) {
                            LOG(LOG_KEYBOARD,LOG_NORMAL)("Intellimouse 4/5-button mode enabled");
                            keyb.ps2mouse.intellimouse_btn45=true;
                        }
                    }
                }
            }
            break;
        case ACMD_SET_RESOLUTION:
            keyb.aux_command = ACMD_NONE;
            KEYBOARD_AddBuffer(AUX|0xfa);   /* ack */
            keyb.ps2mouse.resolution = val & 3;
            LOG(LOG_KEYBOARD,LOG_NORMAL)("PS/2 mouse resolution set to %u",(int)(1 << (val&3)));
            break;
    }
}

#include "control.h"

bool allow_keyb_reset = true;

void On_Software_CPU_Reset();

static void write_p60(Bitu port,Bitu val,Bitu iolen) {
    (void)port;//UNUSED
    (void)iolen;//UNUSED
    switch (keyb.command) {
    case CMD_NONE:  /* None */
        if (keyb.reset)
            return;

        /* No active command this would normally get sent to the keyboard then */
        KEYBOARD_ClrBuffer();
        switch (val) {
        case 0xed:  /* Set Leds */
            keyb.command=CMD_SETLEDS;
            KEYBOARD_AddBuffer(0xfa);   /* Acknowledge */
            break;
        case 0xee:  /* Echo */
            KEYBOARD_AddBuffer(0xee);   /* Echo */
            break;
        case 0xf0:  /* set scancode set */
            keyb.command=CMD_SETSCANSET;
            KEYBOARD_AddBuffer(0xfa);   /* Acknowledge */
            break;
        case 0xf2:  /* Identify keyboard */
            KEYBOARD_AddBuffer(0xfa);   /* Acknowledge */
            KEYBOARD_AddBuffer(0xab);   /* ID */
            KEYBOARD_AddBuffer(0x83);
            break;
        case 0xf3: /* Typematic rate programming */
            keyb.command=CMD_SETTYPERATE;
            KEYBOARD_AddBuffer(0xfa);   /* Acknowledge */
            break;
        case 0xf4:  /* Enable keyboard,clear buffer, start scanning */
            LOG(LOG_KEYBOARD,LOG_NORMAL)("Clear buffer, enable scanning");
            KEYBOARD_AddBuffer(0xfa);   /* Acknowledge */
            keyb.scanning=true;
            break;
        case 0xf5:   /* Reset keyboard and disable scanning */
            LOG(LOG_KEYBOARD,LOG_NORMAL)("Reset, disable scanning");            
            keyb.scanning=false;
            KEYBOARD_AddBuffer(0xfa);   /* Acknowledge */
            break;
        case 0xf6:  /* Reset keyboard and enable scanning */
            LOG(LOG_KEYBOARD,LOG_NORMAL)("Reset, enable scanning");
            keyb.scanning=true;     /* JC: Original DOSBox code was wrong, this command enables scanning */
            KEYBOARD_AddBuffer(0xfa);   /* Acknowledge */
            break;
        case 0xff:      /* keyboard resets take a long time (about 250ms), most keyboards flash the LEDs during reset */
            KEYBOARD_Reset();
            KEYBOARD_Add8042Response(0xFA); /* ACK */
            KEYBOARD_Add8042Response(0xAA); /* SELF TEST OK (TODO: Need delay!) */
            keyb.reset=true;
            KEYBOARD_SetLEDs(7); /* most keyboard I test with tend to flash the LEDs during reset */
            PIC_AddEvent(KEYBOARD_ResetDelay,RESETDELAY);
            break;
        default:
            /* Just always acknowledge strange commands */
            LOG(LOG_KEYBOARD,LOG_ERROR)("60:Unhandled command %X",(int)val);
            KEYBOARD_AddBuffer(0xfa);   /* Acknowledge */
        }
        return;
    case CMD_SETSCANSET:
        keyb.command=CMD_NONE;
        KEYBOARD_AddBuffer(0xfa);   /* Acknowledge */
        if (val == 0) { /* just asking */
            if (keyb.cb_xlat) {
                switch (keyb.scanset) {
                    case 1: KEYBOARD_AddBuffer(0x43); break;
                    case 2: KEYBOARD_AddBuffer(0x41); break;
                    case 3: KEYBOARD_AddBuffer(0x3F); break;
                }
            }
            else {
                KEYBOARD_AddBuffer(keyb.scanset);
            }
        }
        else {
            if (val > 3) val = 3;
            keyb.scanset = val;
        }
        break;
    case CMD_WRITEAUX:
        keyb.command=CMD_NONE;
        KEYBOARD_AUX_Write(val);
        break;
    case CMD_WRITEOUTPUT:
        keyb.command=CMD_NONE;
        KEYBOARD_ClrBuffer();
        KEYBOARD_AddBuffer(val);    /* and now you return the byte as if it were typed in */
        break;
    case CMD_WRITEAUXOUT:
        KEYBOARD_AddBuffer(AUX|val); /* stuff into AUX output */
        break;
    case CMD_SETOUTPORT:
        if (!(val & 1)) {
            if (allow_keyb_reset) {
                LOG_MSG("Restart by keyboard controller requested\n");
                On_Software_CPU_Reset();
            }
            else {
                LOG_MSG("WARNING: Keyboard output port written with bit 1 clear. Is the guest OS or application attempting to reset the system?\n");
            }
        }
        MEM_A20_Enable((val & 2)>0);
        keyb.command = CMD_NONE;
        break;
    case CMD_SETTYPERATE: 
        if (keyb.reset)
            return;

        {
            static const unsigned int delay[] = { 250, 500, 750, 1000 };
            static const unsigned int repeat[] =
            { 33,37,42,46,50,54,58,63,67,75,83,92,100,
              109,118,125,133,149,167,182,200,217,233,
              250,270,303,333,370,400,435,476,500 };
            keyb.repeat.pause = delay[(val >> 5) & 3];
            keyb.repeat.rate = repeat[val & 0x1f];
            keyb.command = CMD_NONE;
            KEYBOARD_ClrBuffer();
            KEYBOARD_AddBuffer(0xfa);   /* Acknowledge */
        }
        break;
    case CMD_SETLEDS:
        if (keyb.reset)
            return;

        keyb.command=CMD_NONE;
        KEYBOARD_ClrBuffer();
        KEYBOARD_AddBuffer(0xfa);   /* Acknowledge */
        KEYBOARD_SetLEDs(val&7);
        break;
    case CMD_SETCOMMAND: /* 8042 command, not keyboard */
        /* TODO: If biosps2=true and aux=false, disallow the guest OS from changing AUX port parameters including IRQ */
        keyb.command=CMD_NONE;
        keyb.cb_xlat = (val >> 6) & 1;
        keyb.auxactive = !((val >> 5) & 1);
        keyb.active = !((val >> 4) & 1);
        keyb.cb_sys = (val >> 2) & 1;
        keyb.cb_irq12 = (val >> 1) & 1;
        keyb.cb_irq1 = (val >> 0) & 1;
        if (keyb.used && !keyb.scheduled && !keyb.p60changed && keyb.active) {
            keyb.scheduled=true;
            PIC_AddEvent(KEYBOARD_TransferBuffer,KEYDELAY);
        }
        break;
    }
}

static uint8_t port_61_data = 0;

static Bitu read_p61(Bitu, Bitu) {
    unsigned char dbg;
    dbg = ((port_61_data & 0xF) |
        ((TIMER_GetOutput2() || (port_61_data&1) == 0)? 0x20:0) | // NTS: Timer 2 doesn't cycle if Port 61 gate turned off, and it becomes '1' when turned off
        ((fmod(PIC_FullIndex(),0.030) > 0.015)? 0x10:0));
    return dbg;
}

static void write_p61(Bitu, Bitu val, Bitu) {
    uint8_t diff = port_61_data ^ (uint8_t)val;
    if (diff & 0x1) TIMER_SetGate2(val & 0x1);
    if ((diff & 0x3) && !IS_PC98_ARCH) {
        bool pit_clock_gate_enabled = !!(val & 0x1);
        bool pit_output_enabled = !!(val & 0x2);
        PCSPEAKER_SetType(pit_clock_gate_enabled, pit_output_enabled);
    }
    port_61_data = val;
}

static Bitu read_p62(Bitu /*port*/,Bitu /*iolen*/) {
    uint8_t ret = uint8_t(~0x20u);
    if (TIMER_GetOutput2()) ret|=0x20u;
    return ret;
}

static void write_p64(Bitu port,Bitu val,Bitu iolen) {
    (void)port;//UNUSED
    (void)iolen;//UNUSED
    if (keyb.reset)
        return;

    switch (val) {
    case 0x20:      /* read command byte */
        /* TODO: If biosps2=true and aux=false, mask AUX port bits as if AUX isn't there */
        KEYBOARD_Add8042Response(
            (keyb.cb_xlat << 6)      | ((!keyb.auxactive) << 5) |
            ((!keyb.active) << 4)    | (keyb.cb_sys << 2) |
            (keyb.cb_irq12 << 1)     | (keyb.cb_irq1?1:0));
        break;
    case 0x60:
        keyb.command=CMD_SETCOMMAND;
        break;
    case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
    case 0x98: case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: case 0x9f:
        /* TODO: If bit 0 == 0, trigger system reset */
        break;
    case 0xa7:      /* disable aux */
        /* TODO: If biosps2=true and aux=false do not respond */
        if (keyb.enable_aux) {
            //keyb.auxactive=false;
            //LOG(LOG_KEYBOARD,LOG_NORMAL)("AUX De-Activated");
        }
        break;
    case 0xa8:      /* enable aux */
        /* TODO: If biosps2=true and aux=false do not respond */
        if (keyb.enable_aux) {
            keyb.auxactive=true;
            if (keyb.used && !keyb.scheduled && !keyb.p60changed) {
                keyb.scheduled=true;
                PIC_AddEvent(KEYBOARD_TransferBuffer,KEYDELAY);
            }
            LOG(LOG_KEYBOARD,LOG_NORMAL)("AUX Activated");
        }
        break;
    case 0xa9:      /* mouse interface test */
        /* TODO: If biosps2=true and aux=false do not respond */
        KEYBOARD_Add8042Response(0x00); /* OK */
        break;
    case 0xaa:      /* Self test */
        keyb.active=false; /* on real h/w it also seems to disable the keyboard */
        KEYBOARD_Add8042Response(0xaa); /* OK */
        break;
    case 0xab:      /* interface test */
        keyb.active=false; /* on real h/w it also seems to disable the keyboard */
        KEYBOARD_Add8042Response(0x00); /* no error */
        break;
    case 0xae:      /* Activate keyboard */
        keyb.active=true;
        if (keyb.used && !keyb.scheduled && !keyb.p60changed) {
            keyb.scheduled=true;
            PIC_AddEvent(KEYBOARD_TransferBuffer,KEYDELAY);
        }
        LOG(LOG_KEYBOARD,LOG_NORMAL)("Activated");
        break;
    case 0xad:      /* Deactivate keyboard */
        keyb.active=false;
        LOG(LOG_KEYBOARD,LOG_NORMAL)("De-Activated");
        break;
    case 0xc0:      /* read input buffer */
        KEYBOARD_Add8042Response(0x40);
        break;
    case 0xd0:      /* Outport on buffer */
        KEYBOARD_SetPort60((MEM_A20_Enabled() ? 0x02 : 0) | 0x01/*some programs read the output port then write it back*/);
        break;
    case 0xd1:      /* Write to outport */
        keyb.command=CMD_SETOUTPORT;
        break;
    case 0xd2:      /* write output register */
        keyb.command=CMD_WRITEOUTPUT;
        break;
    case 0xd3:      /* write AUX output */
        if (keyb.enable_aux)
            keyb.command=CMD_WRITEAUXOUT;
        else if (aux_warning++ == 0)
            LOG(LOG_KEYBOARD,LOG_ERROR)("Program is writing 8042 AUX. If you intend to use PS/2 mouse emulation you may consider adding aux=1 to your dosbox.conf");
        break;
    case 0xd4:      /* send byte to AUX */
        if (keyb.enable_aux)
            keyb.command=CMD_WRITEAUX;
        else if (aux_warning++ == 0)
            LOG(LOG_KEYBOARD,LOG_ERROR)("Program is writing 8042 AUX. If you intend to use PS/2 mouse emulation you may consider adding aux=1 to your dosbox.conf");
        break;
    case 0xe0:      /* read test port */
        KEYBOARD_Add8042Response(0x00);
        break;
    case 0xf0: case 0xf1: case 0xf2: case 0xf3: case 0xf4: case 0xf5: case 0xf6: case 0xf7:
    case 0xf8: case 0xf9: case 0xfa: case 0xfb: case 0xfc: case 0xfd: case 0xfe: case 0xff:
        /* pulse output register */
        if (!(val & 1)) {
            if (allow_keyb_reset) {
                LOG_MSG("Restart by keyboard controller requested\n");
                On_Software_CPU_Reset();
            }
            else {
                LOG_MSG("WARNING: Keyboard output port written (pulsed) with bit 1 clear. Is the guest OS or application attempting to reset the system?\n");
            }
        }
        break;
    default:
        LOG(LOG_KEYBOARD,LOG_ERROR)("Port 64 write with val %d",(int)val);
        break;
    }
}

static Bitu read_p64(Bitu port,Bitu iolen) {
    (void)port;//UNUSED
    (void)iolen;//UNUSED
    uint8_t status= 0x1c | (keyb.p60changed?0x1:0x0) | (keyb.auxchanged?0x20:0x00);
    return status;
}

void KEYBOARD_AddKey3(KBD_KEYS keytype,bool pressed) {
    uint8_t ret=0,ret2=0;

    if (keyb.reset)
        return;

    /* if the keyboard is disabled, then store the keystroke but don't transmit yet */
    /*
    if (!keyb.active || !keyb.scanning) {
        keyb.pending_key = keytype;
        keyb.pending_key_state = pressed;
        return;
    }
    */

    switch (keytype) {
    case KBD_kor_hancha:
        keyb.repeat.key=KBD_NONE;
        keyb.repeat.wait=0;
        if (!pressed) return;
        KEYBOARD_AddBuffer(0xF1);
        break;
    case KBD_kor_hanyong:
        keyb.repeat.key=KBD_NONE;
        keyb.repeat.wait=0;
        if (!pressed) return;
        KEYBOARD_AddBuffer(0xF2);
        break;
    case KBD_esc:ret=0x08;break;
    case KBD_1:ret=0x16;break;
    case KBD_2:ret=0x1E;break;
    case KBD_3:ret=0x26;break;      
    case KBD_4:ret=0x25;break;
    case KBD_5:ret=0x2e;break;
    case KBD_6:ret=0x36;break;      
    case KBD_7:ret=0x3d;break;
    case KBD_8:ret=0x3e;break;
    case KBD_9:ret=0x46;break;      
    case KBD_0:ret=0x45;break;

    case KBD_minus:ret=0x4e;break;
    case KBD_equals:ret=0x55;break;
    case KBD_kpequals:ret=0x0F;break; /* According to Battler */
    case KBD_backspace:ret=0x66;break;
    case KBD_tab:ret=0x0d;break;

    case KBD_q:ret=0x15;break;      
    case KBD_w:ret=0x1d;break;
    case KBD_e:ret=0x24;break;      
    case KBD_r:ret=0x2d;break;
    case KBD_t:ret=0x2c;break;      
    case KBD_y:ret=0x35;break;
    case KBD_u:ret=0x3c;break;      
    case KBD_i:ret=0x43;break;
    case KBD_o:ret=0x44;break;      
    case KBD_p:ret=0x4d;break;

    case KBD_leftbracket:ret=0x54;break;
    case KBD_rightbracket:ret=0x5b;break;
    case KBD_enter:ret=0x5a;break;
    case KBD_leftctrl:ret=0x11;break;

    case KBD_a:ret=0x1c;break;
    case KBD_s:ret=0x1b;break;
    case KBD_d:ret=0x23;break;
    case KBD_f:ret=0x2b;break;
    case KBD_g:ret=0x34;break;      
    case KBD_h:ret=0x33;break;      
    case KBD_j:ret=0x3b;break;
    case KBD_k:ret=0x42;break;      
    case KBD_l:ret=0x4b;break;

    case KBD_semicolon:ret=0x4c;break;
    case KBD_quote:ret=0x52;break;
    case KBD_jp_hankaku:ret=0x0e;break;
    case KBD_grave:ret=0x0e;break;
    case KBD_leftshift:ret=0x12;break;
    case KBD_backslash:ret=0x5c;break;
    case KBD_z:ret=0x1a;break;
    case KBD_x:ret=0x22;break;
    case KBD_c:ret=0x21;break;
    case KBD_v:ret=0x2a;break;
    case KBD_b:ret=0x32;break;
    case KBD_n:ret=0x31;break;
    case KBD_m:ret=0x3a;break;

    case KBD_comma:ret=0x41;break;
    case KBD_period:ret=0x49;break;
    case KBD_slash:ret=0x4a;break;
    case KBD_rightshift:ret=0x59;break;
    case KBD_kpmultiply:ret=0x7e;break;
    case KBD_leftalt:ret=0x19;break;
    case KBD_space:ret=0x29;break;
    case KBD_capslock:ret=0x14;break;

    case KBD_f1:ret=0x07;break;
    case KBD_f2:ret=0x0f;break;
    case KBD_f3:ret=0x17;break;
    case KBD_f4:ret=0x1f;break;
    case KBD_f5:ret=0x27;break;
    case KBD_f6:ret=0x2f;break;
    case KBD_f7:ret=0x37;break;
    case KBD_f8:ret=0x3f;break;
    case KBD_f9:ret=0x47;break;
    case KBD_f10:ret=0x4f;break;
    case KBD_f11:ret=0x56;break;
    case KBD_f12:ret=0x5e;break;

    /* IBM F13-F24 = Shift F1-F12 */
    case KBD_f13:ret=0x12;ret2=0x07;break;
    case KBD_f14:ret=0x12;ret2=0x0F;break;
    case KBD_f15:ret=0x12;ret2=0x17;break;
    case KBD_f16:ret=0x12;ret2=0x1F;break;
    case KBD_f17:ret=0x12;ret2=0x27;break;
    case KBD_f18:ret=0x12;ret2=0x2F;break;
    case KBD_f19:ret=0x12;ret2=0x37;break;
    case KBD_f20:ret=0x12;ret2=0x3F;break;
    case KBD_f21:ret=0x12;ret2=0x47;break;
    case KBD_f22:ret=0x12;ret2=0x4F;break;
    case KBD_f23:ret=0x12;ret2=0x56;break;
    case KBD_f24:ret=0x12;ret2=0x5E;break;

    case KBD_numlock:ret=0x76;break;
    case KBD_scrolllock:ret=0x5f;break;

    case KBD_kp7:ret=0x6c;break;
    case KBD_kp8:ret=0x75;break;
    case KBD_kp9:ret=0x7d;break;
    case KBD_kpminus:ret=0x84;break;
    case KBD_kp4:ret=0x6b;break;
    case KBD_kp5:ret=0x73;break;
    case KBD_kp6:ret=0x74;break;
    case KBD_kpplus:ret=0x7c;break;
    case KBD_kp1:ret=0x69;break;
    case KBD_kp2:ret=0x72;break;
    case KBD_kp3:ret=0x7a;break;
    case KBD_kp0:ret=0x70;break;
    case KBD_kpperiod:ret=0x71;break;

/*  case KBD_extra_lt_gt:ret=;break; */

    //The Extended keys

    case KBD_kpenter:ret=0x79;break;
    case KBD_rightctrl:ret=0x58;break;
    case KBD_kpdivide:ret=0x77;break;
    case KBD_rightalt:ret=0x39;break;
    case KBD_home:ret=0x6e;break;
    case KBD_up:ret=0x63;break;
    case KBD_pageup:ret=0x6f;break;
    case KBD_left:ret=0x61;break;
    case KBD_right:ret=0x6a;break;
    case KBD_end:ret=0x65;break;
    case KBD_down:ret=0x60;break;
    case KBD_pagedown:ret=0x6d;break;
    case KBD_insert:ret=0x67;break;
    case KBD_delete:ret=0x64;break;
    case KBD_pause:ret=0x62;break;
    case KBD_printscreen:ret=0x57;break;
    case KBD_lwindows:ret=0x8B;break;
    case KBD_rwindows:ret=0x8C;break;
    case KBD_rwinmenu:ret=0x8D;break;
    case KBD_jp_muhenkan:ret=0x85;break;
    case KBD_jp_henkan:ret=0x86;break;
    case KBD_jp_hiragana:ret=0x87;break;/*also Katakana */
    default:
        LOG(LOG_MISC, LOG_WARN)("Unsupported key press %lu", (unsigned long)keytype);
        return;
    }

    /* Add the actual key in the keyboard queue */
    if (pressed) {
        if (keyb.repeat.key==keytype) keyb.repeat.wait=keyb.repeat.rate;        
        else keyb.repeat.wait=keyb.repeat.pause;
        keyb.repeat.key=keytype;
    } else {
        if (keytype >= KBD_f13 && keytype <= KBD_f24) {
            unsigned int t = ret;
            ret = ret2;
            ret2 = t;
        }

        keyb.repeat.key=KBD_NONE;
        keyb.repeat.wait=0;
    }

    if (!pressed) KEYBOARD_AddBuffer(0xf0);
    KEYBOARD_AddBuffer(ret);
    if (ret2 != 0) {
        if (!pressed) KEYBOARD_AddBuffer(0xf0);
        KEYBOARD_AddBuffer(ret2);
    }
}

void KEYBOARD_AddKey2(KBD_KEYS keytype,bool pressed) {
    uint8_t ret=0,ret2=0;bool extend=false;

    if (keyb.reset)
        return;

    /* if the keyboard is disabled, then store the keystroke but don't transmit yet */
    /*if (!keyb.active || !keyb.scanning) {
        keyb.pending_key = keytype;
        keyb.pending_key_state = pressed;
        return;
    }*/

    switch (keytype) {
    case KBD_kor_hancha:
        keyb.repeat.key=KBD_NONE;
        keyb.repeat.wait=0;
        if (!pressed) return;
        KEYBOARD_AddBuffer(0xF1);
        break;
    case KBD_kor_hanyong:
        keyb.repeat.key=KBD_NONE;
        keyb.repeat.wait=0;
        if (!pressed) return;
        KEYBOARD_AddBuffer(0xF2);
        break;
    case KBD_esc:ret=0x76;break;
    case KBD_1:ret=0x16;break;
    case KBD_2:ret=0x1e;break;
    case KBD_3:ret=0x26;break;      
    case KBD_4:ret=0x25;break;
    case KBD_5:ret=0x2e;break;
    case KBD_6:ret=0x36;break;      
    case KBD_7:ret=0x3d;break;
    case KBD_8:ret=0x3e;break;
    case KBD_9:ret=0x46;break;      
    case KBD_0:ret=0x45;break;

    case KBD_minus:ret=0x4e;break;
    case KBD_equals:ret=0x55;break;
    case KBD_kpequals:ret=0x0F;break; /* According to Battler */
    case KBD_backspace:ret=0x66;break;
    case KBD_tab:ret=0x0d;break;

    case KBD_q:ret=0x15;break;      
    case KBD_w:ret=0x1d;break;
    case KBD_e:ret=0x24;break;      
    case KBD_r:ret=0x2d;break;
    case KBD_t:ret=0x2c;break;      
    case KBD_y:ret=0x35;break;
    case KBD_u:ret=0x3c;break;      
    case KBD_i:ret=0x43;break;
    case KBD_o:ret=0x44;break;      
    case KBD_p:ret=0x4d;break;

    case KBD_leftbracket:ret=0x54;break;
    case KBD_rightbracket:ret=0x5b;break;
    case KBD_enter:ret=0x5a;break;
    case KBD_leftctrl:ret=0x14;break;

    case KBD_a:ret=0x1c;break;
    case KBD_s:ret=0x1b;break;
    case KBD_d:ret=0x23;break;
    case KBD_f:ret=0x2b;break;
    case KBD_g:ret=0x34;break;      
    case KBD_h:ret=0x33;break;      
    case KBD_j:ret=0x3b;break;
    case KBD_k:ret=0x42;break;      
    case KBD_l:ret=0x4b;break;

    case KBD_semicolon:ret=0x4c;break;
    case KBD_quote:ret=0x52;break;
    case KBD_jp_hankaku:ret=0x0e;break;
    case KBD_grave:ret=0x0e;break;
    case KBD_leftshift:ret=0x12;break;
    case KBD_backslash:ret=0x5d;break;
    case KBD_z:ret=0x1a;break;
    case KBD_x:ret=0x22;break;
    case KBD_c:ret=0x21;break;
    case KBD_v:ret=0x2a;break;
    case KBD_b:ret=0x32;break;
    case KBD_n:ret=0x31;break;
    case KBD_m:ret=0x3a;break;

    case KBD_comma:ret=0x41;break;
    case KBD_period:ret=0x49;break;
    case KBD_slash:ret=0x4a;break;
    case KBD_rightshift:ret=0x59;break;
    case KBD_kpmultiply:ret=0x7c;break;
    case KBD_leftalt:ret=0x11;break;
    case KBD_space:ret=0x29;break;
    case KBD_capslock:ret=0x58;break;

    case KBD_f1:ret=0x05;break;
    case KBD_f2:ret=0x06;break;
    case KBD_f3:ret=0x04;break;
    case KBD_f4:ret=0x0c;break;
    case KBD_f5:ret=0x03;break;
    case KBD_f6:ret=0x0b;break;
    case KBD_f7:ret=0x83;break;
    case KBD_f8:ret=0x0a;break;
    case KBD_f9:ret=0x01;break;
    case KBD_f10:ret=0x09;break;
    case KBD_f11:ret=0x78;break;
    case KBD_f12:ret=0x07;break;

    /* IBM F13-F24 = Shift F1-F12 */
    case KBD_f13:ret=0x12;ret2=0x05;break;
    case KBD_f14:ret=0x12;ret2=0x06;break;
    case KBD_f15:ret=0x12;ret2=0x04;break;
    case KBD_f16:ret=0x12;ret2=0x0c;break;
    case KBD_f17:ret=0x12;ret2=0x03;break;
    case KBD_f18:ret=0x12;ret2=0x0b;break;
    case KBD_f19:ret=0x12;ret2=0x83;break;
    case KBD_f20:ret=0x12;ret2=0x0a;break;
    case KBD_f21:ret=0x12;ret2=0x01;break;
    case KBD_f22:ret=0x12;ret2=0x09;break;
    case KBD_f23:ret=0x12;ret2=0x78;break;
    case KBD_f24:ret=0x12;ret2=0x07;break;

    case KBD_numlock:ret=0x77;break;
    case KBD_scrolllock:ret=0x7e;break;

    case KBD_kp7:ret=0x6c;break;
    case KBD_kp8:ret=0x75;break;
    case KBD_kp9:ret=0x7d;break;
    case KBD_kpminus:ret=0x7b;break;
    case KBD_kp4:ret=0x6b;break;
    case KBD_kp5:ret=0x73;break;
    case KBD_kp6:ret=0x74;break;
    case KBD_kpplus:ret=0x79;break;
    case KBD_kp1:ret=0x69;break;
    case KBD_kp2:ret=0x72;break;
    case KBD_kp3:ret=0x7a;break;
    case KBD_kp0:ret=0x70;break;
    case KBD_kpperiod:ret=0x71;break;

/*  case KBD_extra_lt_gt:ret=;break; */

    //The Extended keys

    case KBD_kpenter:extend=true;ret=0x5a;break;
    case KBD_rightctrl:extend=true;ret=0x14;break;
    case KBD_kpdivide:extend=true;ret=0x4a;break;
    case KBD_rightalt:extend=true;ret=0x11;break;
    case KBD_home:extend=true;ret=0x6c;break;
    case KBD_up:extend=true;ret=0x75;break;
    case KBD_pageup:extend=true;ret=0x7d;break;
    case KBD_left:extend=true;ret=0x6b;break;
    case KBD_right:extend=true;ret=0x74;break;
    case KBD_end:extend=true;ret=0x69;break;
    case KBD_down:extend=true;ret=0x72;break;
    case KBD_pagedown:extend=true;ret=0x7a;break;
    case KBD_insert:extend=true;ret=0x70;break;
    case KBD_delete:extend=true;ret=0x71;break;
    case KBD_pause:
        KEYBOARD_AddBuffer(0xe1);
        KEYBOARD_AddBuffer(0x14);
        KEYBOARD_AddBuffer(0x77);
        KEYBOARD_AddBuffer(0xe1);
        KEYBOARD_AddBuffer(0xf0);
        KEYBOARD_AddBuffer(0x14);
        KEYBOARD_AddBuffer(0xf0);
        KEYBOARD_AddBuffer(0x77);
        return;
    case KBD_printscreen:
        extend=true;
        if (pressed) { ret=0x12; ret2=0x7c; }
        else         { ret=0x7c; ret2=0x12; }
        return;
    case KBD_lwindows:extend=true;ret=0x1f;break;
    case KBD_rwindows:extend=true;ret=0x27;break;
    case KBD_rwinmenu:extend=true;ret=0x2f;break;
    case KBD_jp_muhenkan:ret=0x67;break;
    case KBD_jp_henkan:ret=0x64;break;
    case KBD_jp_hiragana:ret=0x13;break;/*also Katakana */
    default:
        LOG(LOG_MISC, LOG_WARN)("Unsupported key press %lu", (unsigned long)keytype);
        return;
    }
    /* Add the actual key in the keyboard queue */
    if (pressed) {
        if (keyb.repeat.key==keytype) keyb.repeat.wait=keyb.repeat.rate;        
        else keyb.repeat.wait=keyb.repeat.pause;
        keyb.repeat.key=keytype;
    } else {
        if (keytype >= KBD_f13 && keytype <= KBD_f24) {
            unsigned int t = ret;
            ret = ret2;
            ret2 = t;
        }

        keyb.repeat.key=KBD_NONE;
        keyb.repeat.wait=0;
    }

    if (extend) KEYBOARD_AddBuffer(0xe0);
    if (!pressed) KEYBOARD_AddBuffer(0xf0);
    KEYBOARD_AddBuffer(ret);
    if (ret2 != 0) {
        if (extend) KEYBOARD_AddBuffer(0xe0); 
        if (!pressed) KEYBOARD_AddBuffer(0xf0);
        KEYBOARD_AddBuffer(ret2);
    }
}

bool pc98_caps(void);
bool pc98_kana(void);
void pc98_caps_toggle(void);
void pc98_kana_toggle(void);
void pc98_numlock_toggle(void);
void pc98_keyboard_send(const unsigned char b);
bool pc98_force_ibm_layout = false;

/* this version sends to the PC-98 8251 emulation NOT the AT 8042 emulation */
void KEYBOARD_PC98_AddKey(KBD_KEYS keytype,bool pressed) {
    uint8_t ret=0;

    switch (keytype) {                          // NAME or
                                                // NM SH KA KA+SH       NM=no-mod SH=shift KA=kana KA+SH=kana+shift
    case KBD_esc:           ret=0x00;break;     // ESC
    case KBD_1:             ret=0x01;break;     // 1  !  ヌ
    case KBD_2:             ret=0x02;break;     // 2  "  フ
    case KBD_3:             ret=0x03;break;     // 3  #  ア ァ
    case KBD_4:             ret=0x04;break;     // 4  $  ウ ゥ
    case KBD_5:             ret=0x05;break;     // 5  %  エ ェ
    case KBD_6:             ret=0x06;break;     // 6  &  オ ォ
    case KBD_7:             ret=0x07;break;     // 7  '  ヤ ャ
    case KBD_8:             ret=0x08;break;     // 8  (  ユ ュ
    case KBD_9:             ret=0x09;break;     // 9  )  ヨ ョ
    case KBD_0:             ret=0x0A;break;     // 0     ワ ヲ
    case KBD_minus:         ret=0x0B;break;     // -  =  ホ
    case KBD_equals:        ret=0x0C;break;     // ^  `  ヘ             US keyboard layout hack
    case KBD_caret:         ret=0x0C;break;     // ^  `  ヘ
    case KBD_backslash:     ret=0x0D;break;     // ¥  |  ｰ
    case KBD_jp_yen:        ret=0x0D;break;     // ¥  |  ｰ
    case KBD_backspace:     ret=0x0E;break;     // BS (BACKSPACE)
    case KBD_tab:           ret=0x0F;break;     // TAB
    case KBD_q:             ret=0x10;break;     // q  Q  タ
    case KBD_w:             ret=0x11;break;     // w  W  テ
    case KBD_e:             ret=0x12;break;     // e  E  イ ィ
    case KBD_r:             ret=0x13;break;     // r  R  ス
    case KBD_t:             ret=0x14;break;     // t  T  カ
    case KBD_y:             ret=0x15;break;     // y  Y  ン
    case KBD_u:             ret=0x16;break;     // u  U  ナ
    case KBD_i:             ret=0x17;break;     // i  I  ニ
    case KBD_o:             ret=0x18;break;     // o  O  ラ
    case KBD_p:             ret=0x19;break;     // p  P  セ
    case KBD_atsign:        ret=0x1A;break;     // @  ~  ﾞ
    case KBD_leftbracket:   ret=0x1B;break;     // [  {  ﾟ  ｢
    case KBD_enter:         ret=0x1C;break;     // ENTER/RETURN
    case KBD_kpenter:       ret=0x1C;break;     // ENTER/RETURN (KEYPAD)
    case KBD_a:             ret=0x1D;break;     // a  A  チ
    case KBD_s:             ret=0x1E;break;     // s  S  ト
    case KBD_d:             ret=0x1F;break;     // d  D  シ
    case KBD_f:             ret=0x20;break;     // f  F  ハ
    case KBD_g:             ret=0x21;break;     // g  G  キ
    case KBD_h:             ret=0x22;break;     // h  H  ク
    case KBD_j:             ret=0x23;break;     // j  J  マ
    case KBD_k:             ret=0x24;break;     // k  K  ノ
    case KBD_l:             ret=0x25;break;     // l  L  リ
    case KBD_semicolon:     ret=0x26;break;     // ;  +  レ
    case KBD_quote:         ret=0x27;break;     // :  *  ケ         American US keyboard layout hack
    case KBD_colon:         ret=0x27;break;     // :  *  ケ
    case KBD_rightbracket:  ret=0x28;break;     // ]  }  ム ｣
    case KBD_z:             ret=0x29;break;     // z  Z  ツ ッ
    case KBD_x:             ret=0x2A;break;     // x  X  サ
    case KBD_c:             ret=0x2B;break;     // c  C  ソ
    case KBD_v:             ret=0x2C;break;     // v  V  ヒ
    case KBD_b:             ret=0x2D;break;     // b  B  コ
    case KBD_n:             ret=0x2E;break;     // n  N  ミ
    case KBD_m:             ret=0x2F;break;     // m  M  モ
    case KBD_comma:         ret=0x30;break;     // ,  <  ネ ､
    case KBD_period:        ret=0x31;break;     // .  >  ル ｡
    case KBD_slash:         ret=0x32;break;     // /  ?  メ ･
    case KBD_jp_ro:         ret=0x33;break;     //    _  ロ
    case KBD_space:         ret=0x34;break;     // SPACEBAR
    case KBD_xfer:          ret=0x35;break;     // XFER
    case KBD_pageup:        ret=0x36;break;     // ROLL UP
    case KBD_pagedown:      ret=0x37;break;     // ROLL DOWN
    case KBD_insert:        ret=0x38;break;     // INS
    case KBD_delete:        ret=0x39;break;     // DEL
    case KBD_up:            ret=0x3A;break;     // UP ARROW
    case KBD_left:          ret=0x3B;break;     // LEFT ARROW
    case KBD_right:         ret=0x3C;break;     // RIGHT ARROW
    case KBD_down:          ret=0x3D;break;     // DOWN ARROW
    case KBD_home:          ret=0x3E;break;     // HOME / CLR
    case KBD_help:          ret=0x3F;break;     // HELP
    case KBD_kpminus:       ret=0x40;break;     // - (KEYPAD)
    case KBD_kpdivide:      ret=0x41;break;     // / (KEYPAD)
    case KBD_kp7:           ret=0x42;break;     // 7 (KEYPAD)
    case KBD_kp8:           ret=0x43;break;     // 8 (KEYPAD)
    case KBD_kp9:           ret=0x44;break;     // 9 (KEYPAD)
    case KBD_kpmultiply:    ret=0x45;break;     // * (KEYPAD)
    case KBD_kp4:           ret=0x46;break;     // 4 (KEYPAD)
    case KBD_kp5:           ret=0x47;break;     // 5 (KEYPAD)
    case KBD_kp6:           ret=0x48;break;     // 6 (KEYPAD)
    case KBD_kpplus:        ret=0x49;break;     // + (KEYPAD)
    case KBD_kp1:           ret=0x4A;break;     // 1 (KEYPAD)
    case KBD_kp2:           ret=0x4B;break;     // 2 (KEYPAD)
    case KBD_kp3:           ret=0x4C;break;     // 3 (KEYPAD)
    case KBD_kpequals:      ret=0x4D;break;     // = (KEYPAD)
    case KBD_kp0:           ret=0x4E;break;     // 0 (KEYPAD)
    case KBD_kpcomma:       ret=0x4F;break;     // , (KEYPAD)
    case KBD_kpperiod:      ret=0x50;break;     // . (KEYPAD)
    case KBD_nfer:          ret=0x51;break;     // NFER
    case KBD_vf1:           ret=0x52;break;     // vf･1
    case KBD_vf2:           ret=0x53;break;     // vf･2
    case KBD_vf3:           ret=0x54;break;     // vf･3
    case KBD_vf4:           ret=0x55;break;     // vf･4
    case KBD_vf5:           ret=0x56;break;     // vf･5
    case KBD_stop:          ret=0x60;break;     // STOP
    case KBD_copy:          ret=0x61;break;     // COPY
    case KBD_f1:            ret=0x62;break;     // f･1
    case KBD_f2:            ret=0x63;break;     // f･2
    case KBD_f3:            ret=0x64;break;     // f･3
    case KBD_f4:            ret=0x65;break;     // f･4
    case KBD_f5:            ret=0x66;break;     // f･5
    case KBD_f6:            ret=0x67;break;     // f･6
    case KBD_f7:            ret=0x68;break;     // f･7
    case KBD_f8:            ret=0x69;break;     // f･8
    case KBD_f9:            ret=0x6A;break;     // f･9
    case KBD_f10:           ret=0x6B;break;     // f･10
    case KBD_leftshift:     ret=0x70;break;     // SHIFT
    case KBD_rightshift:    ret=0x70;break;     // SHIFT
    case KBD_leftalt:       ret=0x73;break;     // GRPH (handled by Windows as if ALT key)
    case KBD_rightalt:      ret=0x73;break;     // GRPH (handled by Windows as if ALT key)
    case KBD_leftctrl:      ret=0x74;break;     // CTRL
    case KBD_rightctrl:     ret=0x74;break;     // CTRL
    case KBD_grave:
        if(pc98_force_ibm_layout)
            ret=0x1A; //HACK, reuse @ key
        break;
    
    case KBD_capslock:                          // CAPS
        if (pressed) {                          // sends only on keypress, does not resend if held down
            pc98_caps_toggle();
            pc98_keyboard_send(0x71 | (!pc98_caps() ? 0x80 : 0x00)); // make code if caps switched on, break if caps switched off
        }
        return;

    case KBD_numlock:                           // NUM
        pc98_numlock_toggle();
        return;

    case KBD_kana:                              // KANA
        if (pressed) {                          // sends only on keypress, does not resend if held down
            pc98_kana_toggle();
            pc98_keyboard_send(0x72 | (!pc98_kana() ? 0x80 : 0x00)); // make code if caps switched on, break if caps switched off
        }
        return;

    default: return;
    }

    /* PC-98 keyboards appear to repeat make/break codes when the key is held down */
    if (pressed && keyb.repeat.key == keytype)
        pc98_keyboard_send(ret | 0x80);

    /* Add the actual key in the keyboard queue */
    if (pressed) {
        if (keyb.repeat.key == keytype) keyb.repeat.wait = keyb.repeat.rate;        
        else keyb.repeat.wait = keyb.repeat.pause;
        keyb.repeat.key = keytype;
    } else {
        if (keyb.repeat.key == keytype) {
            /* repeated key being released */
            keyb.repeat.key  = KBD_NONE;
            keyb.repeat.wait = 0;
        }
    }

    if (!pressed) ret |= 0x80;

    pc98_keyboard_send(ret | (!pressed ? 0x80 : 0x00));
}

void KEYBOARD_AddKey1(KBD_KEYS keytype,bool pressed) {
    uint8_t ret=0,ret2=0;bool extend=false;

    if (keyb.reset)
        return;

    /* if the keyboard is disabled, then store the keystroke but don't transmit yet */
    /*if (!keyb.active || !keyb.scanning) {
        keyb.pending_key = keytype;
        keyb.pending_key_state = pressed;
        return;
    }*/

    switch (keytype) {
    case KBD_kor_hancha:
        keyb.repeat.key=KBD_NONE;
        keyb.repeat.wait=0;
        if (!pressed) return;
        KEYBOARD_AddBuffer(0xF1);
        break;
    case KBD_kor_hanyong:
        keyb.repeat.key=KBD_NONE;
        keyb.repeat.wait=0;
        if (!pressed) return;
        KEYBOARD_AddBuffer(0xF2);
        break;
    case KBD_esc:ret=1;break;
    case KBD_1:ret=2;break;
    case KBD_2:ret=3;break;
    case KBD_3:ret=4;break;     
    case KBD_4:ret=5;break;
    case KBD_5:ret=6;break;
    case KBD_6:ret=7;break;     
    case KBD_7:ret=8;break;
    case KBD_8:ret=9;break;
    case KBD_9:ret=10;break;        
    case KBD_0:ret=11;break;

    case KBD_minus:ret=12;break;
    case KBD_equals:ret=13;break;
    case KBD_kpequals:ret=0x59;break; /* According to Battler */
    case KBD_backspace:ret=14;break;
    case KBD_tab:ret=15;break;

    case KBD_q:ret=16;break;        
    case KBD_w:ret=17;break;
    case KBD_e:ret=18;break;        
    case KBD_r:ret=19;break;
    case KBD_t:ret=20;break;        
    case KBD_y:ret=21;break;
    case KBD_u:ret=22;break;        
    case KBD_i:ret=23;break;
    case KBD_o:ret=24;break;        
    case KBD_p:ret=25;break;

    case KBD_leftbracket:ret=26;break;
    case KBD_rightbracket:ret=27;break;
    case KBD_enter:ret=28;break;
    case KBD_leftctrl:
        ret=29;
        keyb.leftctrl_pressed=pressed;
        break;

    case KBD_a:ret=30;break;
    case KBD_s:ret=31;break;
    case KBD_d:ret=32;break;
    case KBD_f:ret=33;break;
    case KBD_g:ret=34;break;        
    case KBD_h:ret=35;break;        
    case KBD_j:ret=36;break;
    case KBD_k:ret=37;break;        
    case KBD_l:ret=38;break;

    case KBD_semicolon:ret=39;break;
    case KBD_quote:ret=40;break;
    case KBD_jp_hankaku:ret=41;break;
    case KBD_grave:ret=41;break;
    case KBD_leftshift:ret=42;break;
    case KBD_backslash:ret=43;break;
    case KBD_z:ret=44;break;
    case KBD_x:ret=45;break;
    case KBD_c:ret=46;break;
    case KBD_v:ret=47;break;
    case KBD_b:ret=48;break;
    case KBD_n:ret=49;break;
    case KBD_m:ret=50;break;

    case KBD_comma:ret=51;break;
    case KBD_period:ret=52;break;
    case KBD_slash:ret=53;break;
    case KBD_rightshift:ret=54;break;
    case KBD_kpmultiply:ret=55;break;
    case KBD_leftalt:ret=56;break;
    case KBD_space:ret=57;break;
    case KBD_capslock:ret=58;break;

    case KBD_f1:ret=59;break;
    case KBD_f2:ret=60;break;
    case KBD_f3:ret=61;break;
    case KBD_f4:ret=62;break;
    case KBD_f5:ret=63;break;
    case KBD_f6:ret=64;break;
    case KBD_f7:ret=65;break;
    case KBD_f8:ret=66;break;
    case KBD_f9:ret=67;break;
    case KBD_f10:ret=68;break;
    case KBD_f11:ret=87;break;
    case KBD_f12:ret=88;break;

    /* IBM F13-F24 apparently map to Shift + F1-F12 */
    case KBD_f13:ret=0x2A;ret2=59;break;
    case KBD_f14:ret=0x2A;ret2=60;break;
    case KBD_f15:ret=0x2A;ret2=61;break;
    case KBD_f16:ret=0x2A;ret2=62;break;
    case KBD_f17:ret=0x2A;ret2=63;break;
    case KBD_f18:ret=0x2A;ret2=64;break;
    case KBD_f19:ret=0x2A;ret2=65;break;
    case KBD_f20:ret=0x2A;ret2=66;break;
    case KBD_f21:ret=0x2A;ret2=67;break;
    case KBD_f22:ret=0x2A;ret2=68;break;
    case KBD_f23:ret=0x2A;ret2=87;break;
    case KBD_f24:ret=0x2A;ret2=88;break;

    case KBD_numlock:ret=69;break;
    case KBD_scrolllock:ret=70;break;

    case KBD_kp7:ret=71;break;
    case KBD_kp8:ret=72;break;
    case KBD_kp9:ret=73;break;
    case KBD_kpminus:ret=74;break;
    case KBD_kp4:ret=75;break;
    case KBD_kp5:ret=76;break;
    case KBD_kp6:ret=77;break;
    case KBD_kpplus:ret=78;break;
    case KBD_kp1:ret=79;break;
    case KBD_kp2:ret=80;break;
    case KBD_kp3:ret=81;break;
    case KBD_kp0:ret=82;break;
    case KBD_kpperiod:ret=83;break;

    case KBD_extra_lt_gt:ret=86;break;

    //The Extended keys

    case KBD_kpenter:extend=true;ret=28;break;
    case KBD_rightctrl:
        extend=true;ret=29;
        keyb.rightctrl_pressed=pressed;
        break;
    case KBD_kpdivide:extend=true;ret=53;break;
    case KBD_rightalt:extend=true;ret=56;break;
    case KBD_home:extend=true;ret=71;break;
    case KBD_up:extend=true;ret=72;break;
    case KBD_pageup:extend=true;ret=73;break;
    case KBD_left:extend=true;ret=75;break;
    case KBD_right:extend=true;ret=77;break;
    case KBD_end:extend=true;ret=79;break;
    case KBD_down:extend=true;ret=80;break;
    case KBD_pagedown:extend=true;ret=81;break;
    case KBD_insert:extend=true;ret=82;break;
    case KBD_delete:extend=true;ret=83;break;
    case KBD_pause:
        if (!pressed) {
            /* keyboards send both make&break codes for this key on
               key press and nothing on key release */
            return;
        }
        if (!keyb.leftctrl_pressed && !keyb.rightctrl_pressed) {
            /* neither leftctrl, nor rightctrl pressed -> PAUSE key */
            KEYBOARD_AddBuffer(0xe1);
            KEYBOARD_AddBuffer(29);
            KEYBOARD_AddBuffer(69);
            KEYBOARD_AddBuffer(0xe1);
            KEYBOARD_AddBuffer(29|0x80);
            KEYBOARD_AddBuffer(69|0x80);
        } else if (!keyb.leftctrl_pressed || !keyb.rightctrl_pressed) {
            /* exactly one of [leftctrl, rightctrl] is pressed -> Ctrl+BREAK */
            KEYBOARD_AddBuffer(0xe0);
            KEYBOARD_AddBuffer(70);
            KEYBOARD_AddBuffer(0xe0);
            KEYBOARD_AddBuffer(70|0x80);
        }
        /* pressing this key also disables any previous key repeat */
        keyb.repeat.key=KBD_NONE;
        keyb.repeat.wait=0;
        return;
    case KBD_printscreen:
        /* NTS: Check previous assertion that the Print Screen sent these bytes in
         *      one order when pressed and reverse order when released. Or perhaps
         *      that's only what some keyboards do. --J.C. */
        KEYBOARD_AddBuffer(0xe0);
        KEYBOARD_AddBuffer(0x2a | (pressed ? 0 : 0x80)); /* 0x2a == 42 */
        KEYBOARD_AddBuffer(0xe0);
        KEYBOARD_AddBuffer(0x37 | (pressed ? 0 : 0x80)); /* 0x37 == 55 */
        /* pressing this key also disables any previous key repeat */
        keyb.repeat.key = KBD_NONE;
        keyb.repeat.wait = 0;
        return;
    case KBD_lwindows:extend=true;ret=0x5B;break;
    case KBD_rwindows:extend=true;ret=0x5C;break;
    case KBD_rwinmenu:extend=true;ret=0x5D;break;
    case KBD_jp_muhenkan:ret=0x7B;break;
    case KBD_jp_henkan:ret=0x79;break;
    case KBD_jp_hiragana:ret=0x70;break;/*also Katakana */
    case KBD_jp_backslash:ret=0x73;break;/*JP 106-key: _ \ or ろ (ro)  <-- WARNING: UTF-8 unicode */
    case KBD_jp_yen:ret=0x7d;break;/*JP 106-key: | ¥ (yen) or ー (prolonged sound mark)  <-- WARNING: UTF-8 unicode */
    default:
        LOG(LOG_MISC, LOG_WARN)("Unsupported key press %lu", (unsigned long)keytype);
        return;
    }

    /* Add the actual key in the keyboard queue */
    if (pressed) {
        if (keyb.repeat.key == keytype) keyb.repeat.wait = keyb.repeat.rate;        
        else keyb.repeat.wait = keyb.repeat.pause;
        keyb.repeat.key = keytype;
    } else {
        if (keyb.repeat.key == keytype) {
            /* repeated key being released */
            keyb.repeat.key  = KBD_NONE;
            keyb.repeat.wait = 0;
        }

        if (keytype >= KBD_f13 && keytype <= KBD_f24) {
            unsigned int t = ret;
            ret = ret2;
            ret2 = t;
        }

        ret += 128;
        if (ret2 != 0) ret2 += 128;
    }
    if (extend) KEYBOARD_AddBuffer(0xe0);
    KEYBOARD_AddBuffer(ret);
    if (ret2 != 0) {
        if (extend) KEYBOARD_AddBuffer(0xe0); 
        KEYBOARD_AddBuffer(ret2);
    }
}

static void KEYBOARD_TickHandler(void) {
    if (keyb.reset)
        return;

    if (keyb.active && keyb.scanning) {
        if (keyb.pending_key >= 0) {
            KEYBOARD_AddKey((KBD_KEYS)keyb.pending_key,keyb.pending_key_state);
            keyb.pending_key = -1;
        }
        else if (keyb.repeat.wait) {
            keyb.repeat.wait--;
            if (!keyb.repeat.wait) KEYBOARD_AddKey(keyb.repeat.key,true);
        }
    }
}

void KEYBOARD_AddKey(KBD_KEYS keytype,bool pressed) {
    if (IS_PC98_ARCH) {
        KEYBOARD_PC98_AddKey(keytype,pressed);
    }
    else if (keyb.cb_xlat) {
        /* emulate typical setup where keyboard generates scan set 2 and controller translates to scan set 1 */
        /* yeah I know... yuck */
        KEYBOARD_AddKey1(keytype,pressed);
    }
    else {
        switch (keyb.scanset) {
            case 1: KEYBOARD_AddKey1(keytype,pressed); break;
            case 2: KEYBOARD_AddKey2(keytype,pressed); break;
            case 3: KEYBOARD_AddKey3(keytype,pressed); break;
        }
    }
}
    
static void KEYBOARD_ShutDown(Section * sec) {
    (void)sec;//UNUSED
    TIMER_DelTickHandler(&KEYBOARD_TickHandler);
}

bool KEYBOARD_Report_BIOS_PS2Mouse() {
    return keyb.enable_aux && (keyb.ps2mouse.type != MOUSE_NONE);
}

static IO_ReadHandleObject ReadHandler_8255_PC98[4];
static IO_WriteHandleObject WriteHandler_8255_PC98[4];

static IO_ReadHandleObject ReadHandler_8255prn_PC98[4];
static IO_WriteHandleObject WriteHandler_8255prn_PC98[4];

static IO_WriteHandleObject Reset_PC98;

extern bool gdc_5mhz_mode;
extern bool gdc_5mhz_mode_initial;

//! \brief PC-98 Printer 8255 PPI emulation (Intel 8255A device)
//!
//! \description TODO...
//!
//!              This PPI is connected to I/O ports 0x40-0x46 even.
//!              - 0x40 Port A
//!              - 0x42 Port B
//!              - 0x44 Port C
//!              - 0x46 control/mode
//!
//!              Except on super high resolution
//!              display units, Port B is also used to
//!              return some dip switches and system configuration,
//!              with only one bit to indicate printer status.
//!
//!              This PPI is connected to:
//!              - Port A (out): Output to printer
//!              - Port B (in):  PC-98 model, system clock (8 or 5/10MHz),
//!                              HGC graphics extension, printer busy,
//!                              V30/V33 vs Intel x86 CPU, CPU HA/LT sys type,
//!                              VF flag (?)
//!              - Port C (out): Strobe, IR8 interrupt enable, 287/387 reset(?)
class PC98_Printer_8255 : public Intel8255 {
public:
    PC98_Printer_8255() : Intel8255() {
        ppiName = "Printer 8255";
        portNames[PortA] = "Printer output";
        portNames[PortB] = "System configuration";
        portNames[PortC] = "Strobe and controls";
        pinNames[PortA][0] = "Latch bit 0";
        pinNames[PortA][1] = "Latch bit 1";
        pinNames[PortA][2] = "Latch bit 2";
        pinNames[PortA][3] = "Latch bit 3";
        pinNames[PortA][4] = "Latch bit 4";
        pinNames[PortA][5] = "Latch bit 5";
        pinNames[PortA][6] = "Latch bit 6";
        pinNames[PortA][7] = "Latch bit 7";
        pinNames[PortB][0] = "VF VF flag";
        pinNames[PortB][1] = "CPUT operation CPU (V30 if set)";
        pinNames[PortB][2] = "Printer busy signal";
        pinNames[PortB][3] = "HGC graphics extension function DIP SW 1-8";
        pinNames[PortB][4] = "LCD plasma display usage condition DIP SW 1-3";
        pinNames[PortB][5] = "System clock (5/10mhz or 8mhz)";
        pinNames[PortB][6] = "System type, bit 0";
        pinNames[PortB][7] = "System type, bit 1";
        pinNames[PortC][0] = "unused";
        pinNames[PortC][1] = "Reset 287/387 by CPU reset if set";
        pinNames[PortC][2] = "unused";
        pinNames[PortC][3] = "IR8 interrupt request ON/OFF";
        pinNames[PortC][4] = "unused";
        pinNames[PortC][5] = "unused";
        pinNames[PortC][6] = "unused";
        pinNames[PortC][7] = "Printer strobe output";
    }
    virtual ~PC98_Printer_8255() {
    }
public:
    /* TODO: Writes to Port A should go to printer emulation */
    /* TODO: Writes to bit 7, Port C should go to printer emulation (strobe pin) */
    /* port B is input */
    virtual uint8_t inPortB(void) const {
        return      0x80 +                                                          /* bits [7:6]   10 = other model */
                    ((PIT_TICK_RATE == PIT_TICK_RATE_PC98_8MHZ) ? 0x20 : 0x00) +    /* bit  [5:5]   1 = 8MHz  0 = 5/10MHz */
                    0x10 +                                                          /* bit  [4:4]   1 = LCD plasma display usage cond. not used */
                    0x04;                                                           /* bit  [2:2]   printer busy signal (1=inactive) */
        /* NTS: also returns:
         *       bit  [3:3]     0 = HGC graphics extension function extended mode
         *       bit  [1:1]     0 = 80x86 processor (not V30)
         *       bit  [0:0]     0 = other models (?) */
    }
};

static PC98_Printer_8255 pc98_prn_8255;

bool PC98_SHUT0=true,PC98_SHUT1=true;

//! \brief PC-98 System 8255 PPI emulation (Intel 8255A device)
//!
//! \description NEC PC-98 systems use a 8255 to enable reading
//!              the DIP switches on the front (port A),
//!              some hardware state (Port B), and to control
//!              some hardware and signal the BIOS for
//!              shutdown/reset (Port C).
//!
//!              This PPI is connected to I/O ports 0x31-0x37 odd.
//!              - 0x31 Port A
//!              - 0x33 Port B
//!              - 0x35 Port C
//!              - 0x37 control/mode
//!
//!              Note that on real hardware, PC-9801 systems
//!              had physical DIP switches until sometime around
//!              the early nineties when the DIP switches became
//!              virtual, and were set by the BIOS according to
//!              configuration stored elsewhere.
//!
//!              This PPI is connected to:
//!              - Port A (in): DIP switches 2-1 through 2-8
//!              - Port B (in): Parity error, expansion bus INT 3,
//!                             and RS-232C (8251) status
//!              - Port C (out): RS-232C interrupt enable, speaker (buzzer)
//!                              inhibit, parity check enable, and
//!                              shutdown flags.
//!
//!              The two "shutdown" bit flags serve to instruct
//!              the BIOS on what to do after a soft CPU reset.
//!              One particular setting allows 286-class systems
//!              to reset to real mode through a FAR return stack,
//!              for the same reasons that IBM PC/AT systems enable
//!              reset to real mode through a reset byte written
//!              to RTC CMOS and a far pointer to jump to.
//!              A well known PC-9821 expanded memory emulator,
//!              VEMM486.EXE, relies on the shutdown bits in that
//!              manner when it initializes.
class PC98_System_8255 : public Intel8255 {
public:
    PC98_System_8255() : Intel8255() {
        ppiName = "System 8255";
        portNames[PortA] = "DIP switches 2-1 through 2-8";
        portNames[PortB] = "Various system status";
        portNames[PortC] = "System control bits";
        pinNames[PortA][0] = "DIP switch 2-1";
        pinNames[PortA][1] = "DIP switch 2-2";
        pinNames[PortA][2] = "DIP switch 2-3";
        pinNames[PortA][3] = "DIP switch 2-4";
        pinNames[PortA][4] = "DIP switch 2-5";
        pinNames[PortA][5] = "DIP switch 2-6";
        pinNames[PortA][6] = "DIP switch 2-7";
        pinNames[PortA][7] = "DIP switch 2-8";
        pinNames[PortB][0] = "Read data of calendar clock (CDAT)";
        pinNames[PortB][1] = "Expansion RAM parity error (EMCK)";
        pinNames[PortB][2] = "Internal RAM parity error (IMCK)";
        pinNames[PortB][3] = "DIP switch 1-1 High resolution CRT type";
        pinNames[PortB][4] = "Expansion bus INT 3 signal";
        pinNames[PortB][5] = "RS-232C CD signal";
        pinNames[PortB][6] = "RS-232C CS signal";
        pinNames[PortB][7] = "RS-232C CI signal";
        pinNames[PortC][0] = "RS-232C enable RXRDY interrupt";
        pinNames[PortC][1] = "RS-232C enable TXEMPTY interrupt";
        pinNames[PortC][2] = "RS-232C enable TXRDY interrupt";
        pinNames[PortC][3] = "Buzzer inhibit";
        pinNames[PortC][4] = "RAM parity check enable";
        pinNames[PortC][5] = "Shutdown flag 1"; /* <- 286 or later */
        pinNames[PortC][6] = "PSTB printer signal inhibit (mask if set)";
        pinNames[PortC][7] = "Shutdown flag 0"; /* <- 286 or later */
    }
    virtual ~PC98_System_8255() {
    }
public:
    /* port A is input */
    virtual uint8_t inPortA(void) const {
        /* TODO: Improve this! What do the various 2-1 to 2-8 switches do?
         *       It might help to look at the BIOS setup menus of 1990s PC-98 systems
         *       that offer toggling virtual versions of these DIP switches to see
         *       what the BIOS menu text says. */
        /* NTS: Return the INITIAL setting of the GDC. Guest applications (like Windows 3.1)
         *      can and will change it later. This must reflect the initial setting as if
         *      what the BIOS had initially intended. */
        return 0x63 | (gdc_5mhz_mode_initial ? 0x00 : 0x80); // taken from a PC-9821 Lt2
    }
    /* port B is input */
    virtual uint8_t inPortB(void) const {
        /* TODO: Improve this! */
        return 0xF9; // taken from a PC-9821 Lt2
    }
    /* port C is output (both halves) */
    virtual void outPortC(const uint8_t mask) {
        if (mask & 0x80) /* Shutdown flag 0 */
            PC98_SHUT0 = !!(latchOutPortC & 0x80);

        if (mask & 0x20) /* Shutdown flag 1 */
            PC98_SHUT1 = !!(latchOutPortC & 0x20);

        if (mask & 0x08) { /* PC speaker aka "buzzer". Note this bit is an inhibit, set to zero to turn on */
            port_61_data = (latchOutPortC & 0x08) ? 0 : 3;
            TIMER_SetGate2(!!port_61_data);
            PCSPEAKER_SetType(!!port_61_data,!!port_61_data);
        }
    }
};

static PC98_System_8255 pc98_sys_8255;

static void pc98_reset_write(Bitu port,Bitu val,Bitu /*iolen*/) {
    (void)port;//UNUSED
    LOG_MSG("Restart by port F0h requested: val=%02x SHUT0=%u SHUT1=%u\n",(unsigned int)val,PC98_SHUT0,PC98_SHUT1);
    On_Software_CPU_Reset();
}

static void pc98_8255_write(Bitu port,Bitu val,Bitu /*iolen*/) {
    /* 0x31-0x37 odd */
    pc98_sys_8255.writeByPort((port - 0x31) >> 1U,val);
}

static Bitu pc98_8255_read(Bitu port,Bitu /*iolen*/) {
    /* 0x31-0x37 odd */
    return pc98_sys_8255.readByPort((port - 0x31) >> 1U);
}

static void pc98_8255prn_write(Bitu port,Bitu val,Bitu /*iolen*/) {
    /* 0x31-0x37 odd */
    pc98_prn_8255.writeByPort((port - 0x40) >> 1U,val);
}

static Bitu pc98_8255prn_read(Bitu port,Bitu /*iolen*/) {
    /* 0x31-0x37 odd */
    return pc98_prn_8255.readByPort((port - 0x40) >> 1U);
}

static struct pc98_keyboard {
    pc98_keyboard() : caps(false), kana(false), num(false) {
    }

    bool                        caps;
    bool                        kana;
    bool                        num;
} pc98_keyboard_state;

bool pc98_caps(void) {
    return pc98_keyboard_state.caps;
}

bool pc98_kana(void) {
    return pc98_keyboard_state.kana;
}

void pc98_caps_toggle(void) {
    pc98_keyboard_state.caps = !pc98_keyboard_state.caps;
}

void pc98_kana_toggle(void) {
    pc98_keyboard_state.kana = !pc98_keyboard_state.kana;
}

void pc98_numlock_toggle(void) {
    pc98_keyboard_state.num = !pc98_keyboard_state.num;
}

void uart_rx_load(Bitu val);
void uart_tx_load(Bitu val);
void pc98_keyboard_recv_byte(Bitu val);

static struct pc98_8251_keyboard_uart {
    enum cmdreg_state {
        MODE_STATE=0,
        SYNC_CHAR1,
        SYNC_CHAR2,
        COMMAND_STATE
    };

    unsigned char               data;
    unsigned char               txdata;
    enum cmdreg_state           state;
    unsigned char               mode_byte;
    bool                        keyboard_reset;
    bool                        rx_enable;
    bool                        tx_enable;
    bool                        valid_state;

    bool                        rx_busy;
    bool                        rx_ready;
    bool                        tx_busy;
    bool                        tx_empty;

    /* io_delay in milliseconds for use with PIC delay code */
    double                      io_delay_ms;
    double                      tx_load_ms;

    /* recv data from keyboard */
    unsigned char               recv_buffer[32] = {};
    unsigned char               recv_in,recv_out;

    pc98_8251_keyboard_uart() : data(0xFF), txdata(0xFF), state(MODE_STATE), mode_byte(0), keyboard_reset(false), rx_enable(false), tx_enable(false), valid_state(false), rx_busy(false), rx_ready(false), tx_busy(false), tx_empty(true), recv_in(0), recv_out(0) {
        io_delay_ms = (((1/*start*/+8/*data*/+1/*parity*/+1/*stop*/) * 1000.0) / 19200);
        tx_load_ms = (((1/*start*/+8/*data*/) * 1000.0) / 19200);
    }

    void reset(void) {
        PIC_RemoveEvents(uart_tx_load);
        PIC_RemoveEvents(uart_rx_load);
        PIC_RemoveEvents(pc98_keyboard_recv_byte);

        state = MODE_STATE;
        rx_busy = false;
        rx_ready = false;
        tx_empty = true;
        tx_busy = false;
        mode_byte = 0;
        recv_out = 0;
        recv_in = 0;
    }

    void device_send_data(unsigned char b) {
        unsigned char nidx;

        nidx = (recv_in + 1) % 32;
        if (nidx == recv_out) {
            LOG_MSG("8251 device send recv overrun");
        }
        else {
            recv_buffer[recv_in] = b;
            recv_in = nidx;
        }

        if (!rx_busy) {
            rx_busy = true;
            PIC_AddEvent(uart_rx_load,io_delay_ms,0);
        }
    }

    unsigned char read_data(void) {
        rx_ready = false;
        if (recv_in != recv_out && !rx_busy) {
            rx_busy = true;
            PIC_AddEvent(uart_rx_load,io_delay_ms,0);
        }
        return data;
    }

    void write_data(unsigned char b) {
        if (!valid_state)
            return;

        if (!tx_busy) {
            txdata = b;
            tx_busy = true;

            PIC_AddEvent(uart_tx_load,tx_load_ms,0);
            PIC_AddEvent(pc98_keyboard_recv_byte,io_delay_ms,txdata);
        }
    }

    void tx_load_complete(void) {
        tx_busy = false;
    }

    void rx_load_complete(void) {
        if (!rx_ready) {
            rx_ready = true;
            data = recv_buffer[recv_out];
            recv_out = (recv_out + 1) % 32;

//            LOG_MSG("8251 recv %02X",data);
            PIC_ActivateIRQ(1);

            if (recv_out != recv_in) {
                PIC_AddEvent(uart_rx_load,io_delay_ms,0);
                rx_busy = true;
            }
            else {
                rx_busy = false;
            }
        }
        else {
            LOG_MSG("8251 warning: RX overrun");
            rx_busy = false;
        }
    }

    void xmit_finish(void) {
        tx_empty = true;
        tx_busy = false;
    }

    unsigned char read_status(void) {
        unsigned char r = 0;

        /* bit[7:7] = DSR (1=DSR at zero level)
         * bit[6:6] = syndet/brkdet
         * bit[5:5] = framing error
         * bit[4:4] = overrun error
         * bit[3:3] = parity error
         * bit[2:2] = TxEMPTY
         * bit[1:1] = RxRDY
         * bit[0:0] = TxRDY */
        r |= (!tx_busy ? 0x01 : 0x00) |
             (rx_ready ? 0x02 : 0x00) |
             (tx_empty ? 0x04 : 0x00);

        return r;
    }

    void writecmd(const unsigned char b) { /* write to command register */
        if (state == MODE_STATE) {
            mode_byte = b;

            if ((b&3) != 0) {
                /* bit[7:6] = number of stop bits  (0=invalid 1=1-bit 2=1.5-bit 3=2-bit)
                 * bit[5:5] = even/odd parity      (1=even 0=odd)
                 * bit[4:4] = parity enable        (1=enable 0=disable)
                 * bit[3:2] = character length     (0=5  1=6  2=7  3=8)
                 * bit[1:0] = baud rate factor     (0=sync mode   1=1X   2=16X   3=64X)
                 *
                 * note that "baud rate factor" means how much to divide the baud rate clock to determine
                 * the bit rate that bits are transmitted. Typical PC-98 programming practice is to set
                 * the baud rate clock fed to the chip at 16X the baud rate and then specify 16X baud rate factor. */
                /* async mode */
                state = COMMAND_STATE;

                /* keyboard must operate at 19200 baud 8 bits odd parity 16X baud rate factor */
                valid_state = (b == 0x5E); /* bit[7:0] = 01 0 1 11 10 */
                                           /*            |  | | |  |  */
                                           /*            |  | | |  +---- 16X baud rate factor */
                                           /*            |  | | +------- 8 bits per character */
                                           /*            |  | +--------- parity enable */
                                           /*            |  +----------- odd parity */
                                           /*            +-------------- 1 stop bit */
            }
            else {
                /* bit[7:7] = single character sync(1=single  0=double)
                 * bit[6:6] = external sync detect (1=syndet is an input   0=syndet is an output)
                 * bit[5:5] = even/odd parity      (1=even 0=odd)
                 * bit[4:4] = parity enable        (1=enable 0=disable)
                 * bit[3:2] = character length     (0=5  1=6  2=7  3=8)
                 * bit[1:0] = baud rate factor     (0=sync mode)
                 *
                 * I don't think anything uses the keyboard in this manner, therefore, not supported in this emulation. */
                LOG_MSG("8251 keyboard warning: Mode byte synchronous mode not supported");
                state = SYNC_CHAR1;
                valid_state = false;
            }
        }
        else if (state == COMMAND_STATE) {
            /* bit[7:7] = Enter hunt mode (not used here)
             * bit[6:6] = internal reset (8251 resets, prepares to accept mode byte)
             * bit[5:5] = RTS inhibit (1=force RTS to zero, else RTS reflects RxRDY state of the chip)
             * bit[4:4] = error reset
             * bit[3:3] = send break character (0=normal  1=force TxD low). On PC-98 keyboard this is wired to reset pin of the keyboard CPU.
             * bit[2:2] = receive enable
             * bit[1:1] = DTR inhibit (1=force DTR to zero). Connected to PC-98 RTY pin.
             * bit[0:0] = transmit enable */
            if (b & 0x40) {
                /* internal reset, returns 8251 to mode state */
                state = MODE_STATE;
            }

            /* TODO: Does the 8251 take any other bits if bit 6 was set to reset the 8251? */
            keyboard_reset = !!(b & 0x08);
            rx_enable = !!(b & 0x04);
            tx_enable = !!(b & 0x01);
        }
    }
} pc98_8251_keyboard_uart_state;

void uart_tx_load(Bitu val) {
    (void)val;//UNUSED
    pc98_8251_keyboard_uart_state.tx_load_complete();
}

void uart_rx_load(Bitu val) {
    (void)val;//UNUSED
    pc98_8251_keyboard_uart_state.rx_load_complete();
}

void pc98_keyboard_send(const unsigned char b) {
    pc98_8251_keyboard_uart_state.device_send_data(b);
}

void pc98_keyboard_recv_byte(Bitu val) {
    pc98_8251_keyboard_uart_state.xmit_finish();
    LOG_MSG("PC-98 recv 0x%02x",(unsigned int)val);
}

static Bitu keyboard_pc98_8251_uart_41_read(Bitu port,Bitu /*iolen*/) {
    (void)port;//UNUSED
    return pc98_8251_keyboard_uart_state.read_data();
}

static void keyboard_pc98_8251_uart_41_write(Bitu port,Bitu val,Bitu /*iolen*/) {
    (void)port;//UNUSED
    pc98_8251_keyboard_uart_state.write_data((unsigned char)val);
}

static Bitu keyboard_pc98_8251_uart_43_read(Bitu port,Bitu /*iolen*/) {
    (void)port;//UNUSED
    return pc98_8251_keyboard_uart_state.read_status();
}

static void keyboard_pc98_8251_uart_43_write(Bitu port,Bitu val,Bitu /*iolen*/) {
    (void)port;//UNUSED
    pc98_8251_keyboard_uart_state.writecmd((unsigned char)val);
}

/* called from INT 18h */
void check_keyboard_fire_IRQ1(void) {
    if (pc98_8251_keyboard_uart_state.read_status() & 2/*RxRDY*/)
        PIC_ActivateIRQ(1);
}

int8_t p7fd9_8255_mouse_x = 0;
int8_t p7fd9_8255_mouse_y = 0;
int8_t p7fd9_8255_mouse_x_latch = 0;
int8_t p7fd9_8255_mouse_y_latch = 0;
uint8_t p7fd9_8255_mouse_sel = 0;
uint8_t p7fd9_8255_mouse_latch = 0;
uint8_t p7fd8_8255_mouse_int_enable = 0;

void pc98_mouse_movement_apply(int x,int y) {
    x += p7fd9_8255_mouse_x; if (x < -128) x = -128; if (x > 127) x = 127;
    y += p7fd9_8255_mouse_y; if (y < -128) y = -128; if (y > 127) y = 127;
    p7fd9_8255_mouse_x = (int8_t)x;
    p7fd9_8255_mouse_y = (int8_t)y;
}

void MOUSE_DummyEvent(void);

unsigned int pc98_mouse_rate_hz = 120;

static double pc98_mouse_tick_interval_ms(void) {
    return 1000.0/*ms*/ / pc98_mouse_rate_hz;
}

static double pc98_mouse_time_to_next_tick_ms(void) {
    const double x = pc98_mouse_tick_interval_ms();
    return x - fmod(PIC_FullIndex(),x);
}

extern uint8_t MOUSE_IRQ;

static bool pc98_mouse_tick_scheduled = false;

static void pc98_mouse_tick_event(Bitu val) {
    (void)val;

    if (p7fd8_8255_mouse_int_enable) {
        /* Generate interrupt */
        PIC_ActivateIRQ(MOUSE_IRQ);
        /* keep the periodic interrupt going */
        PIC_AddEvent(pc98_mouse_tick_event,pc98_mouse_tick_interval_ms());
    }
    else
        pc98_mouse_tick_scheduled = false;
}

static void pc98_mouse_tick_schedule(void) {
    if (p7fd8_8255_mouse_int_enable) {
        if (!pc98_mouse_tick_scheduled) {
            pc98_mouse_tick_scheduled = true;
            PIC_AddEvent(pc98_mouse_tick_event,pc98_mouse_time_to_next_tick_ms());
        }
    }
    else {
        /* Don't unschedule the event, the game may un-inhibit the interrupt later.
         * The PIC event will not reschedule itself if inhibited at the time of the signal. */
    }
}

//! \brief PC-98 System Bus Mouse PPI emulation (Intel 8255A device)
//!
//! \description NEC PC-98 systems use a 8255 to interface a bus mouse
//!              to the system.
//!
//!              This PPI is connected to I/O ports 0x7FD9-0x7FDF odd.
//!              - 0x7FD9 is Port A (input)
//!              - 0x7FDB is Port B (input)
//!              - 0x7FDD is Port C (bits[7:4] output, bits[3:0] input)
//!              - 0x7FDF is control/mode
//!
//!              Button state is read directly as 3 bits (left, right, middle).
//!
//!              Mouse movement is latched on command and read 4 bits
//!              (one nibble at a time) to obtain two signed 8-bit
//!              x and y mickey counts (to detect movement).
//!
//!              Mouse data is read from port A.
//!
//!              Interrupt inhibit, selection of the nibble, and the
//!              command to latch mouse movement is done by writing
//!              to port C.
//!
//!              According to some documentation found online, port B
//!              is attached to various unrelated signals and/or status.
//!
//!              The interrupt line is connected to IRQ 13 of the
//!              interrupt controller on PC-98 systems.
//!
//!              There is at least one PC-98 game "Metal Force" known
//!              to abuse this interface as a periodic interrupt source
//!              instead of using it as an interface for mouse input.
class PC98_Mouse_8255 : public Intel8255 {
public:
    PC98_Mouse_8255() : Intel8255() {
        ppiName = "Mouse 8255";
        portNames[PortA] = "Mouse input";
        portNames[PortB] = "TODO";
        portNames[PortC] = "TODO";
        pinNames[PortA][0] = "MD0 (counter latch bit 0)";
        pinNames[PortA][1] = "MD1 (counter latch bit 1)";
        pinNames[PortA][2] = "MD2 (counter latch bit 2)";
        pinNames[PortA][3] = "MD3 (counter latch bit 3)";
        pinNames[PortA][4] = "?";
        pinNames[PortA][5] = "!Right mouse button";
        pinNames[PortA][6] = "!Middle mouse button";
        pinNames[PortA][7] = "!Left mouse button";
        pinNames[PortB][0] = "?";
        pinNames[PortB][1] = "?";
        pinNames[PortB][2] = "?";
        pinNames[PortB][3] = "?";
        pinNames[PortB][4] = "?";
        pinNames[PortB][5] = "?";
        pinNames[PortB][6] = "?";
        pinNames[PortB][7] = "?";
        pinNames[PortC][0] = "?";                               // read
        pinNames[PortC][1] = "?";                               // read
        pinNames[PortC][2] = "DIP SW 3-8 80286 select V30";     // read
        pinNames[PortC][3] = "?";                               // read
        pinNames[PortC][4] = "Mouse interrupt inhibit";         // write
        pinNames[PortC][5] = "SHL, Counter latch upper nibble"; // write
        pinNames[PortC][6] = "SXY, Counter latch Y (X if 0)";   // write
        pinNames[PortC][7] = "Counter latch and clear";         // write
    }
    virtual ~PC98_Mouse_8255() {
    }
public:
    /* port A is input */
    virtual uint8_t inPortA(void) const {
        uint8_t bs;
        Bitu r;

        // bits [7:7] = !(LEFT BUTTON)
        // bits [6:6] = !(MIDDLE BUTTON)
        // bits [5:5] = !(RIGHT BUTTON)
        // bits [4:4] = 0 unused
        // bits [3:0] = 4 bit nibble latched via Port C
        bs = Mouse_GetButtonState();
        r = 0x00;

        if (!(bs & 1)) r |= 0x80;       // left button (inverted bit)
        if (!(bs & 2)) r |= 0x20;       // right button (inverted bit)
        if (!(bs & 4)) r |= 0x40;       // middle button (inverted bit)

        if (!p7fd9_8255_mouse_latch) {
            p7fd9_8255_mouse_x_latch = p7fd9_8255_mouse_x;
            p7fd9_8255_mouse_y_latch = p7fd9_8255_mouse_y;
        }

        switch (p7fd9_8255_mouse_sel) {
            case 0: // X delta
            case 1:
                r |= (uint8_t)(p7fd9_8255_mouse_x_latch >> ((p7fd9_8255_mouse_sel & 1U) * 4U)) & 0xF; // sign extend is intentional
                break;
            case 2: // Y delta
            case 3:
                r |= (uint8_t)(p7fd9_8255_mouse_y_latch >> ((p7fd9_8255_mouse_sel & 1U) * 4U)) & 0xF; // sign extend is intentional
                break;
        }

        return r;
    }
    /* port B is input */
    virtual uint8_t inPortB(void) const {
        /* TODO */
        return 0x00;
    }
    /* port C is input[3:0] and output[7:4] */
    virtual uint8_t inPortC(void) const {
        /* TODO */
        return 0x00;
    }
    /* port C is input[3:0] and output[7:4] */
    virtual void outPortC(const uint8_t mask) {
        if (!enable_pc98_bus_mouse)
            return;

        if (mask & 0x80) { /* bit 7 */
            /* changing from 0 to 1 latches counters and clears them */
            if ((latchOutPortC & 0x80) && !p7fd9_8255_mouse_latch) { // change from 0 to 1 latches counters and clears them
                p7fd9_8255_mouse_x_latch = p7fd9_8255_mouse_x;
                p7fd9_8255_mouse_y_latch = p7fd9_8255_mouse_y;
                p7fd9_8255_mouse_x = 0;
                p7fd9_8255_mouse_y = 0;
            }

            p7fd9_8255_mouse_latch = (latchOutPortC >> 7) & 1;
        }
        if (mask & 0x60) { /* bits 6-5 */
            p7fd9_8255_mouse_sel = (latchOutPortC >> 5) & 3;
        }
        if (mask & 0x10) { /* bit 4 */
            uint8_t p = p7fd8_8255_mouse_int_enable;

            p7fd8_8255_mouse_int_enable = ((latchOutPortC >> 4) & 1) ^ 1; // bit 4 is interrupt MASK

            /* Some games use the mouse interrupt as a periodic source by writing this from the interrupt handler.
             *
             * Does that work on real hardware?? Is this what real hardware does?
             *
             * Games that need this:
             * - Metal Force
             * - Amaranth
             */

            if (p != p7fd8_8255_mouse_int_enable) {
                /* NTS: I'm guessing that if the mouse interrupt has fired, inhibiting the interrupt
                 *      does not remove the IRQ signal already sent.
                 *
                 *      Amaranth's mouse driver appears to rapidly toggle this bit. If we re-fire
                 *      and inhibit the interrupt in reaction the game will get an interrupt rate
                 *      far higher than normal and animation will run way too fast. */
                pc98_mouse_tick_schedule();
            }
        }
    }
};

static PC98_Mouse_8255 pc98_mouse_8255;

//// STUB: PC-98 MOUSE
static void write_p7fd9_mouse(Bitu port,Bitu val,Bitu /*iolen*/) {
    /* 0x7FD9-0x7FDF odd */
    pc98_mouse_8255.writeByPort((port - 0x7FD9) >> 1U,val);
}

static Bitu read_p7fd9_mouse(Bitu port,Bitu /*iolen*/) {
    /* 0x7FD9-0x7FDF odd */
    return pc98_mouse_8255.readByPort((port - 0x7FD9) >> 1U);
}

static void write_pbfdb_mouse(Bitu port,Bitu val,Bitu /*iolen*/) {
    (void)port;

    unsigned int p_pc98_mouse_rate_hz = pc98_mouse_rate_hz;

    /* bits [7:2] = ??
     * bits [1:0] = 120hz clock divider
     *              00 = 120hz (at reset)
     *              01 = 60hz
     *              10 = 30hz
     *              11 = 15hz */
    pc98_mouse_rate_hz = 120u >> (val & 3u);

    if (pc98_mouse_rate_hz != p_pc98_mouse_rate_hz)
        LOG(LOG_MISC,LOG_DEBUG)("PC-98 mouse interrupt rate: %u",pc98_mouse_rate_hz);
}
//////////

void KEYBOARD_OnEnterPC98(Section *sec) {
    (void)sec;//UNUSED

	Section_prop * pc98_section=static_cast<Section_prop *>(control->GetSection("pc98"));
	assert(pc98_section != NULL);
	enable_pc98_bus_mouse = pc98_section->Get_bool("pc-98 bus mouse");

    /* TODO: Keyboard interface change, layout change. */

    /* PC-98 uses the 8255 programmable peripheral interface. Install that here.
     * Sometime in the future, move 8255 emulation to a separate file.
     *
     * The 8255 appears at I/O ports 0x31, 0x33, 0x35, 0x37 */
    if (IS_PC98_ARCH) {
        for (unsigned int i=0;i < 4;i++) {
            ReadHandler_8255_PC98[i].Uninstall();
            WriteHandler_8255_PC98[i].Uninstall();

            ReadHandler_8255prn_PC98[i].Uninstall();
            WriteHandler_8255prn_PC98[i].Uninstall();
        }
        
        pc98_force_ibm_layout = pc98_section->Get_bool("pc-98 force ibm keyboard layout");
        if(pc98_force_ibm_layout)
            LOG_MSG("Forcing PC-98 keyboard to use IBM US-English like default layout");
        mainMenu.get_item("pc98_use_uskb").check(pc98_force_ibm_layout).refresh_item(mainMenu);

    }

    if (!IS_PC98_ARCH) {
        /* remove 60h-63h */
        IO_FreeWriteHandler(0x60,IO_MB);
        IO_FreeReadHandler(0x60,IO_MB);
        IO_FreeWriteHandler(0x61,IO_MB);
        IO_FreeReadHandler(0x61,IO_MB);
        IO_FreeWriteHandler(0x64,IO_MB);
        IO_FreeReadHandler(0x64,IO_MB);
    }   
}

void KEYBOARD_OnEnterPC98_phase2(Section *sec) {
    (void)sec;//UNUSED
    unsigned int i;

    /* Keyboard UART (8251) is at 0x41, 0x43. */
    IO_RegisterWriteHandler(0x41,keyboard_pc98_8251_uart_41_write,IO_MB);
    IO_RegisterReadHandler(0x41,keyboard_pc98_8251_uart_41_read,IO_MB);
    IO_RegisterWriteHandler(0x43,keyboard_pc98_8251_uart_43_write,IO_MB);
    IO_RegisterReadHandler(0x43,keyboard_pc98_8251_uart_43_read,IO_MB);

    /* PC-98 uses the 8255 programmable peripheral interface. Install that here.
     * Sometime in the future, move 8255 emulation to a separate file.
     *
     * The 8255 appears at I/O ports 0x31, 0x33, 0x35, 0x37 */

    /* Port A = input
     * Port B = input
     * Port C = output */
    /* bit[7:7] =  1 = mode set
     * bit[6:5] = 00 = port A mode 0
     * bit[4:4] =  1 = port A input
     * bit[3:3] =  0 = port C upper output              1001 0010 = 0x92
     * bit[2:2] =  0 = port B mode 0
     * bit[1:1] =  1 = port B input
     * bit[0:0] =  0 = port C lower output */
    pc98_sys_8255.writeControl(0x92);
    pc98_sys_8255.writePortC(0xF8); /* SHUT0=1 SHUT1=1 mask printer RAM parity check buzzer inhibit */

    /* Another 8255 is at 0x40-0x46 even for the printer interface */
    /* bit[7:7] =  1 = mode set
     * bit[6:5] = 00 = port A mode 0
     * bit[4:4] =  0 = port A output
     * bit[3:3] =  0 = port C upper output              1000 0010 = 0x82
     * bit[2:2] =  0 = port B mode 0
     * bit[1:1] =  1 = port B input
     * bit[0:0] =  0 = port C lower output */
    pc98_prn_8255.writeControl(0x82);
    pc98_prn_8255.writePortA(0x00); /* printer latch all 0s */
    pc98_prn_8255.writePortC(0x0A); /* 1=IR8 OFF, reset 287/387 by CPU reset */

    for (i=0;i < 4;i++) {
        /* system */
        ReadHandler_8255_PC98[i].Uninstall();
        ReadHandler_8255_PC98[i].Install(0x31 + (i * 2),pc98_8255_read,IO_MB);

        WriteHandler_8255_PC98[i].Uninstall();
        WriteHandler_8255_PC98[i].Install(0x31 + (i * 2),pc98_8255_write,IO_MB);

        /* printer */
        ReadHandler_8255prn_PC98[i].Uninstall();
        ReadHandler_8255prn_PC98[i].Install(0x40 + (i * 2),pc98_8255prn_read,IO_MB);

        WriteHandler_8255prn_PC98[i].Uninstall();
        WriteHandler_8255prn_PC98[i].Install(0x40 + (i * 2),pc98_8255prn_write,IO_MB);
    }

    /* reset port */
    Reset_PC98.Uninstall();
    Reset_PC98.Install(0xF0,pc98_reset_write,IO_MB);

    if (enable_pc98_bus_mouse) {
        /* Mouse */
        for (i=0;i < 4;i++) {
            IO_RegisterWriteHandler(0x7FD9+(i*2),write_p7fd9_mouse,IO_MB);
            IO_RegisterReadHandler(0x7FD9+(i*2),read_p7fd9_mouse,IO_MB);
        }

        /* Mouse control port at BFDB (which can be used to reduce the interrupt rate of the mouse) */
        IO_RegisterWriteHandler(0xBFDB,write_pbfdb_mouse,IO_MB);
    }

    /* Port A = input
     * Port B = input
     * Port C = output */
    /* bit[7:7] =  1 = mode set
     * bit[6:5] = 00 = port A mode 0
     * bit[4:4] =  1 = port A input
     * bit[3:3] =  0 = port C upper output              1001 0010 = 0x92
     * bit[2:2] =  0 = port B mode 0
     * bit[1:1] =  1 = port B input
     * bit[0:0] =  1 = port C lower input */
    pc98_mouse_8255.writeControl(0x93);
    pc98_mouse_8255.writePortC(0x10); /* start with interrupt inhibited. INT 33h emulation will enable it later */
}

extern bool enable_slave_pic;

void KEYBOARD_OnReset(Section *sec) {
    (void)sec;//UNUSED
    Section_prop *section=static_cast<Section_prop *>(control->GetSection("keyboard"));

    LOG(LOG_MISC,LOG_DEBUG)("Keyboard reinitializing");

    if ((keyb.enable_aux=section->Get_bool("aux")) != false) {
        if (machine == MCH_PCJR) {
            keyb.enable_aux = false;
        }
        else {
            LOG(LOG_KEYBOARD,LOG_NORMAL)("Keyboard AUX emulation enabled");
        }
    }

    TIMER_DelTickHandler(&KEYBOARD_TickHandler);

    allow_keyb_reset = section->Get_bool("allow output port reset");

    keyb.ps2mouse.int33_taken = 0;
    keyb.ps2mouse.reset_mode = MM_STREAM; /* NTS: I was wrong: PS/2 mice default to streaming after reset */

    const char * sbtype=section->Get_string("auxdevice");
    keyb.ps2mouse.type = MOUSE_NONE;
    if (sbtype != NULL && machine != MCH_PCJR && enable_slave_pic) {
        if (!strcasecmp(sbtype,"2button"))
            keyb.ps2mouse.type=MOUSE_2BUTTON;
        else if (!strcasecmp(sbtype,"3button"))
            keyb.ps2mouse.type=MOUSE_3BUTTON;
        else if (!strcasecmp(sbtype,"intellimouse"))
            keyb.ps2mouse.type=MOUSE_INTELLIMOUSE;
        else if (!strcasecmp(sbtype,"intellimouse45"))
            keyb.ps2mouse.type=MOUSE_INTELLIMOUSE45;
        else if (!strcasecmp(sbtype,"none"))
            keyb.ps2mouse.type=MOUSE_NONE;
        else {
            keyb.ps2mouse.type=MOUSE_INTELLIMOUSE;
            LOG(LOG_KEYBOARD,LOG_ERROR)("Assuming PS/2 intellimouse, I don't know what '%s' is",sbtype);
        }
    }

    if (IS_PC98_ARCH) {
        KEYBOARD_OnEnterPC98(NULL);
        KEYBOARD_OnEnterPC98_phase2(NULL);
    }
    else {
        IO_RegisterWriteHandler(0x60,write_p60,IO_MB);
        IO_RegisterReadHandler(0x60,read_p60,IO_MB);
        IO_RegisterWriteHandler(0x61,write_p61,IO_MB);
        IO_RegisterReadHandler(0x61,read_p61,IO_MB);
        if (machine==MCH_CGA || machine==MCH_HERC) IO_RegisterReadHandler(0x62,read_p62,IO_MB);
        IO_RegisterWriteHandler(0x64,write_p64,IO_MB);
        IO_RegisterReadHandler(0x64,read_p64,IO_MB);
    }

    TIMER_AddTickHandler(&KEYBOARD_TickHandler);
    write_p61(0,0,0);
    KEYBOARD_Reset();
    AUX_Reset();
    keyb.p60data = 0xAA;
}

void KEYBOARD_Init() {
    LOG(LOG_MISC,LOG_DEBUG)("Initializing keyboard emulation");

    AddExitFunction(AddExitFunctionFuncPair(KEYBOARD_ShutDown));

    AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(KEYBOARD_OnReset));
}

void AUX_Reset() {
    keyb.ps2mouse.mode = keyb.ps2mouse.reset_mode;
    keyb.ps2mouse.acx = 0;
    keyb.ps2mouse.acy = 0;
    keyb.ps2mouse.samplerate = 80;
    keyb.ps2mouse.last_srate[0] = keyb.ps2mouse.last_srate[1] = keyb.ps2mouse.last_srate[2] = 0;
    keyb.ps2mouse.intellimouse_btn45 = false;
    keyb.ps2mouse.intellimouse_mode = false;
    keyb.ps2mouse.reporting = false;
    keyb.ps2mouse.scale21 = false;
    keyb.ps2mouse.resolution = 1;
    if (keyb.ps2mouse.type != MOUSE_NONE && keyb.ps2mouse.int33_taken)
        LOG(LOG_KEYBOARD,LOG_NORMAL)("PS/2 mouse emulation: taking over from INT 33h");
    keyb.ps2mouse.int33_taken = 0;
    keyb.ps2mouse.l = keyb.ps2mouse.m = keyb.ps2mouse.r = false;
}

void AUX_INT33_Takeover() {
    if (keyb.ps2mouse.type != MOUSE_NONE && keyb.ps2mouse.int33_taken)
        LOG(LOG_KEYBOARD,LOG_NORMAL)("PS/2 mouse emulation: Program is using INT 33h, disabling direct AUX emulation");
    keyb.ps2mouse.int33_taken = 1;
}

void KEYBOARD_Reset() {
    /* Init the keyb struct */
    keyb.active=true;
    keyb.scanning=true;
    keyb.pending_key=-1;
    keyb.auxactive=false;
    keyb.pending_key_state=false;
    keyb.command=CMD_NONE;
    keyb.aux_command=ACMD_NONE;
    keyb.p60changed=false;
    keyb.auxchanged=false;
    keyb.led_state = 0x00;
    keyb.repeat.key=KBD_NONE;
    keyb.repeat.pause=500;
    keyb.repeat.rate=33;
    keyb.repeat.wait=0;
    keyb.leftctrl_pressed=false;
    keyb.rightctrl_pressed=false;
    keyb.scanset=1;
    /* command byte */
    keyb.cb_override_inhibit=false;
    keyb.cb_irq12=false;
    keyb.cb_irq1=true;
    keyb.cb_xlat=true;
    keyb.cb_sys=true;
    keyb.reset=false;
    /* OK */
    KEYBOARD_ClrBuffer();
    KEYBOARD_SetLEDs(0);
}

//save state support
void *KEYBOARD_TransferBuffer_PIC_Event = (void*)((uintptr_t)KEYBOARD_TransferBuffer);
void *KEYBOARD_TickHandler_PIC_Timer = (void*)((uintptr_t)KEYBOARD_TickHandler);

namespace
{
class SerializeKeyboard : public SerializeGlobalPOD
{
public:
    SerializeKeyboard() : SerializeGlobalPOD("Keyboard")
    {
        registerPOD(keyb.buffer);
        registerPOD(keyb.used); 
        registerPOD(keyb.pos); 
        registerPOD(keyb.repeat.key); 
        registerPOD(keyb.repeat.wait); 
        registerPOD(keyb.repeat.pause); 
        registerPOD(keyb.repeat.rate); 
        registerPOD(keyb.command); 
        registerPOD(keyb.p60data); 
        registerPOD(keyb.p60changed); 
        registerPOD(keyb.active); 
        registerPOD(keyb.scanning); 
        registerPOD(keyb.scheduled);
        registerPOD(port_61_data);
    }
} dummy;
}
