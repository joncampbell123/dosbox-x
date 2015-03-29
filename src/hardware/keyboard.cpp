/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include "dosbox.h"
#include "keyboard.h"
#include "support.h"
#include "setup.h"
#include "inout.h"
#include "mouse.h"
#include "pic.h"
#include "mem.h"
#include "cpu.h"
#include "mixer.h"
#include "timer.h"
#include <math.h>

#define KEYBUFSIZE 32*3
#define RESETDELAY 400
#define KEYDELAY 0.300f			//Considering 20-30 khz serial clock and 11 bits/char

#define AUX 0x100

void AUX_Reset();
void KEYBOARD_Reset();
static void KEYBOARD_SetPort60(Bit16u val);
static void KEYBOARD_AddBuffer(Bit16u data);
static void KEYBOARD_Add8042Response(Bit8u data);
static void KEYBOARD_SetLEDs(Bit8u bits);
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
	MouseType	type;			/* what kind of mouse we are emulating */
	MouseMode	mode;			/* current mode */
	MouseMode	reset_mode;		/* mode to change to on reset */
	Bit8u		samplerate;		/* current sample rate */
	Bit8u		resolution;		/* current resolution */
	Bit8u		last_srate[3];		/* last 3 "set sample rate" values */
	float		acx,acy;		/* accumulator */
	bool		reporting;		/* reporting */
	bool		scale21;		/* 2:1 scaling */
	bool		intellimouse_mode;	/* intellimouse scroll wheel */
	bool		intellimouse_btn45;	/* 4th & 5th buttons */
	bool		int33_taken;		/* for compatability with existing DOSBox code: allow INT 33H emulation to "take over" and disable us */
	bool		l,m,r;			/* mouse button states */
};

static struct {
	Bit8u buf8042[8];		/* for 8042 responses, taking priority over keyboard responses */
	Bitu buf8042_len;
	Bitu buf8042_pos;
	int pending_key;

	Bit16u buffer[KEYBUFSIZE];
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
	Bit8u p60data;
	Bit8u scanset;
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
	bool cb_irq12;			/* PS/2 mouse */
	bool cb_irq1;
	bool cb_xlat;
	bool cb_sys;
	bool leftctrl_pressed;
	bool rightctrl_pressed;
} keyb;

bool MouseTypeNone() {
	return (keyb.ps2mouse.type == MOUSE_NONE);
}

/* NTS: INT33H emulation is coded to call this ONLY if it hasn't taken over the role of mouse input */
void KEYBOARD_AUX_Event(float x,float y,Bitu buttons) {
	keyb.ps2mouse.acx += x;
	keyb.ps2mouse.acy += y;
	keyb.ps2mouse.l = (buttons & 1)>0;
	keyb.ps2mouse.r = (buttons & 2)>0;
	keyb.ps2mouse.m = (buttons & 4)>0;

	if (keyb.ps2mouse.reporting && keyb.ps2mouse.mode == MM_STREAM) {
		if ((keyb.used+4) < KEYBUFSIZE) {
			int x,y;

			x = (int)(keyb.ps2mouse.acx * (1 << keyb.ps2mouse.resolution));
			x /= 16; /* FIXME: Or else the cursor is WAY too sensitive in Windows 3.1 */
			if (x < -256) x = -256;
			else if (x > 255) x = 255;

			y = -((int)(keyb.ps2mouse.acy * (1 << keyb.ps2mouse.resolution)));
			y /= 16; /* FIXME: Or else the cursor is WAY too sensitive in Windows 3.1 */
			if (y < -256) y = -256;
			else if (y > 255) y = 255;

			KEYBOARD_AddBuffer(AUX|
				((y == -256 || y == 255) ? 0x80 : 0x00) |	/* Y overflow */
				((x == -256 || x == 255) ? 0x40 : 0x00) |	/* X overflow */
				(y & 0x100 ? 0x20 : 0x00) |			/* Y sign bit */
				(x & 0x100 ? 0x10 : 0x00) |			/* X sign bit */
				0x08 |						/* always 1? */
				(keyb.ps2mouse.m ? 4 : 0) |			/* M */
				(keyb.ps2mouse.r ? 2 : 0) |			/* R */
				(keyb.ps2mouse.l ? 1 : 0));			/* L */
			KEYBOARD_AddBuffer(AUX|(x&0xFF));
			KEYBOARD_AddBuffer(AUX|(y&0xFF));
			if (keyb.ps2mouse.intellimouse_btn45) {
				KEYBOARD_AddBuffer(AUX|0x00);			/* TODO: scrollwheel and 4th & 5th buttons */
			}
			else if (keyb.ps2mouse.intellimouse_mode) {
				KEYBOARD_AddBuffer(AUX|0x00);			/* TODO: scrollwheel */
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

static void KEYBOARD_SetPort60(Bit16u val) {
	keyb.auxchanged=(val&AUX)>0;
	keyb.p60changed=true;
	keyb.p60data=val;
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
	keyb.reset=false;
	KEYBOARD_SetLEDs(0);
	KEYBOARD_Add8042Response(0x00);	/* BAT */
}

static void KEYBOARD_TransferBuffer(Bitu val) {
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

static void KEYBOARD_Add8042Response(Bit8u data) {
	if(!keyb.enable_aux) return;
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

static void KEYBOARD_AddBuffer(Bit16u data) {
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

static void KEYBOARD_SetLEDs(Bit8u bits) {
	/* TODO: Maybe someday you could have DOSBox show the LEDs */
	LOG(LOG_KEYBOARD,LOG_NORMAL)("Keyboard LEDs: SCR=%u NUM=%u CAPS=%u",bits&1,(bits>>1)&1,(bits>>2)&1);
}

static Bitu read_p60(Bitu port,Bitu iolen) {
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
	return	(keyb.ps2mouse.mode == MM_REMOTE ? 0x40 : 0x00)|
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
				case 0xff:	/* reset */
					LOG(LOG_KEYBOARD,LOG_NORMAL)("AUX reset");
					KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */
					KEYBOARD_AddBuffer(AUX|0xaa);	/* reset */
					KEYBOARD_AddBuffer(AUX|0x00);	/* i am mouse */
					Mouse_AutoLock(false);
					AUX_Reset();
					break;
				case 0xf6:	/* set defaults */
					KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */
					AUX_Reset();
					break;
				case 0xf5:	/* disable data reporting */
					KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */
					keyb.ps2mouse.reporting = false;
					break;
				case 0xf4:	/* enable data reporting */
					KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */
					keyb.ps2mouse.reporting = true;
					Mouse_AutoLock(true);
					break;
				case 0xf3:	/* set sample rate */
					KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */
					keyb.aux_command = ACMD_SET_RATE;
					break;
				case 0xf2:	/* get device ID */
					KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */

					/* and then the ID */
					if (keyb.ps2mouse.intellimouse_btn45)
						KEYBOARD_AddBuffer(AUX|0x04);
					else if (keyb.ps2mouse.intellimouse_mode)
						KEYBOARD_AddBuffer(AUX|0x03);
					else
						KEYBOARD_AddBuffer(AUX|0x00);
					break;
				case 0xf0:	/* set remote mode */
					KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */
					keyb.ps2mouse.mode = MM_REMOTE;
					break;
				case 0xee:	/* set wrap mode */
					KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */
					keyb.ps2mouse.mode = MM_WRAP;
					break;
				case 0xec:	/* reset wrap mode */
					KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */
					keyb.ps2mouse.mode = MM_REMOTE;
					break;
				case 0xeb:	/* read data */
					KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */
					KEYBOARD_AUX_Event(0,0,
						(keyb.ps2mouse.m << 2)|
						(keyb.ps2mouse.r << 1)|
						(keyb.ps2mouse.l << 0));
					break;
				case 0xea:	/* set stream mode */
					KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */
					keyb.ps2mouse.mode = MM_STREAM;
					break;
				case 0xe9:	/* status request */
					KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */
					KEYBOARD_AddBuffer(AUX|KEYBOARD_AUX_DevStatus());
					KEYBOARD_AddBuffer(AUX|keyb.ps2mouse.resolution);
					KEYBOARD_AddBuffer(AUX|keyb.ps2mouse.samplerate);
					break;
				case 0xe8:	/* set resolution */
					KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */
					keyb.aux_command = ACMD_SET_RESOLUTION;
					break;
				case 0xe7:	/* set scaling 2:1 */
					KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */
					keyb.ps2mouse.scale21 = true;
					LOG(LOG_KEYBOARD,LOG_NORMAL)("PS/2 mouse scaling 2:1");
					break;
				case 0xe6:	/* set scaling 1:1 */
					KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */
					keyb.ps2mouse.scale21 = false;
					LOG(LOG_KEYBOARD,LOG_NORMAL)("PS/2 mouse scaling 1:1");
					break;
			}
			break;
		case ACMD_SET_RATE:
			KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */
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
			KEYBOARD_AddBuffer(AUX|0xfa);	/* ack */
			keyb.ps2mouse.resolution = val & 3;
			LOG(LOG_KEYBOARD,LOG_NORMAL)("PS/2 mouse resolution set to %u",(int)(1 << (val&3)));
			break;
	};
}

#include "control.h"

bool allow_keyb_reset = true;

void On_Software_CPU_Reset();
void restart_program(std::vector<std::string> & parameters);

static void write_p60(Bitu port,Bitu val,Bitu iolen) {
	switch (keyb.command) {
	case CMD_NONE:	/* None */
		if (keyb.reset)
			return;

		/* No active command this would normally get sent to the keyboard then */
		KEYBOARD_ClrBuffer();
		switch (val) {
		case 0xed:	/* Set Leds */
			keyb.command=CMD_SETLEDS;
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			break;
		case 0xee:	/* Echo */
			KEYBOARD_AddBuffer(0xee);	/* JC: The correct response is 0xEE, not 0xFA */
			break;
		case 0xf0:	/* set scancode set */
			keyb.command=CMD_SETSCANSET;
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			break;
		case 0xf2:	/* Identify keyboard */
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			KEYBOARD_AddBuffer(0xab);	/* ID */
			KEYBOARD_AddBuffer(0x83);
			break;
		case 0xf3: /* Typematic rate programming */
			keyb.command=CMD_SETTYPERATE;
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			break;
		case 0xf4:	/* Enable keyboard,clear buffer, start scanning */
			LOG(LOG_KEYBOARD,LOG_NORMAL)("Clear buffer, enable scanning");
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			keyb.scanning=true;
			break;
		case 0xf5:	 /* Reset keyboard and disable scanning */
			LOG(LOG_KEYBOARD,LOG_NORMAL)("Reset, disable scanning");			
			keyb.scanning=false;
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			break;
		case 0xf6:	/* Reset keyboard and enable scanning */
			LOG(LOG_KEYBOARD,LOG_NORMAL)("Reset, enable scanning");
			keyb.scanning=true;		/* JC: Original DOSBox code was wrong, this command enables scanning */
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			break;
		case 0xff:		/* keyboard resets take a long time (about 250ms), most keyboards flash the LEDs during reset */
			KEYBOARD_Reset();
			KEYBOARD_Add8042Response(0xFA);	/* ACK */
			KEYBOARD_Add8042Response(0xAA); /* SELF TEST OK (TODO: Need delay!) */
			keyb.reset=true;
			KEYBOARD_SetLEDs(7); /* most keyboard I test with tend to flash the LEDs during reset */
			PIC_AddEvent(KEYBOARD_ResetDelay,RESETDELAY);
			break;
		default:
			/* Just always acknowledge strange commands */
			LOG(LOG_KEYBOARD,LOG_ERROR)("60:Unhandled command %X",(int)val);
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
		}
		return;
	case CMD_SETSCANSET:
		keyb.command=CMD_NONE;
		if (val == 0) { /* just asking */
			if (keyb.cb_xlat) {
				switch (keyb.scanset) {
					case 1:	KEYBOARD_AddBuffer(0x43); break;
					case 2:	KEYBOARD_AddBuffer(0x41); break;
					case 3:	KEYBOARD_AddBuffer(0x3F); break;
				}
			}
			else {
				KEYBOARD_AddBuffer(keyb.scanset);
			}
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
		}
		else {
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge again */
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
		KEYBOARD_AddBuffer(val);	/* and now you return the byte as if it were typed in */
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
			static const int delay[] = { 250, 500, 750, 1000 };
			static const int repeat[] = 
				{ 33,37,42,46,50,54,58,63,67,75,83,92,100,
				  109,118,125,133,149,167,182,200,217,233,
				  250,270,303,333,370,400,435,476,500 };
			keyb.repeat.pause = delay[(val>>5)&3];
			keyb.repeat.rate = repeat[val&0x1f];
			keyb.command=CMD_NONE;
		}
		/* Fallthrough! as setleds does what we want */
	case CMD_SETLEDS:
		if (keyb.reset)
			return;

		keyb.command=CMD_NONE;
		KEYBOARD_ClrBuffer();
		KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
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

static Bit8u port_61_data = 0;

static Bitu read_p61(Bitu, Bitu) {
	unsigned char dbg;
	dbg = ((port_61_data & 0xF) |
		(TIMER_GetOutput2()? 0x20:0) |
		((fmod(PIC_FullIndex(),0.030) > 0.015)? 0x10:0));
	return dbg;
}

static void write_p61(Bitu, Bitu val, Bitu) {
	Bit8u diff = port_61_data ^ (Bit8u)val;
	if (diff & 0x1) TIMER_SetGate2(val & 0x1);
	if (diff & 0x3) {
		bool pit_clock_gate_enabled = val & 0x1;
		bool pit_output_enabled = val & 0x2;
		PCSPEAKER_SetType(pit_clock_gate_enabled, pit_output_enabled);
	}
	port_61_data = val;
}

static void write_p64(Bitu port,Bitu val,Bitu iolen) {
	if (keyb.reset)
		return;

	switch (val) {
	case 0x20:		/* read command byte */
		/* TODO: If biosps2=true and aux=false, mask AUX port bits as if AUX isn't there */
		KEYBOARD_Add8042Response(
			(keyb.cb_xlat << 6)      | ((!keyb.auxactive) << 5) |
			((!keyb.active) << 4)    | (keyb.cb_sys << 2) |
			(keyb.cb_irq12 << 1)     |  keyb.cb_irq1);
		break;
	case 0x60:
		keyb.command=CMD_SETCOMMAND;
		break;
	case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
	case 0x98: case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: case 0x9f:
		/* TODO: If bit 0 == 0, trigger system reset */
		break;
	case 0xa7:		/* disable aux */
		/* TODO: If biosps2=true and aux=false do not respond */
		if (keyb.enable_aux) {
			//keyb.auxactive=false;
			//LOG(LOG_KEYBOARD,LOG_NORMAL)("AUX De-Activated");
		}
		break;
	case 0xa8:		/* enable aux */
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
	case 0xa9:		/* mouse interface test */
		/* TODO: If biosps2=true and aux=false do not respond */
		KEYBOARD_Add8042Response(0x00); /* OK */
		break;
	case 0xaa:		/* Self test */
		keyb.active=false; /* on real h/w it also seems to disable the keyboard */
		KEYBOARD_Add8042Response(0xaa); /* OK */
		break;
	case 0xab:		/* interface test */
		keyb.active=false; /* on real h/w it also seems to disable the keyboard */
		KEYBOARD_Add8042Response(0x00); /* no error */
		break;
	case 0xae:		/* Activate keyboard */
		keyb.active=true;
		if (keyb.used && !keyb.scheduled && !keyb.p60changed) {
			keyb.scheduled=true;
			PIC_AddEvent(KEYBOARD_TransferBuffer,KEYDELAY);
		}
		LOG(LOG_KEYBOARD,LOG_NORMAL)("Activated");
		break;
	case 0xad:		/* Deactivate keyboard */
		keyb.active=false;
		LOG(LOG_KEYBOARD,LOG_NORMAL)("De-Activated");
		break;
	case 0xc0:		/* read input buffer */
		KEYBOARD_Add8042Response(0x40);
		break;
	case 0xd0:		/* Outport on buffer */
		KEYBOARD_SetPort60((MEM_A20_Enabled() ? 0x02 : 0) | 0x01/*some programs read the output port then write it back*/);
		break;
	case 0xd1:		/* Write to outport */
		keyb.command=CMD_SETOUTPORT;
		break;
	case 0xd2:		/* write output register */
		keyb.command=CMD_WRITEOUTPUT;
		break;
	case 0xd3:		/* write AUX output */
		if (keyb.enable_aux)
			keyb.command=CMD_WRITEAUXOUT;
		else if (aux_warning++ == 0)
			LOG(LOG_KEYBOARD,LOG_ERROR)("Program is writing 8042 AUX. If you intend to use PS/2 mouse emulation you may consider adding aux=1 to your dosbox.conf");
		break;
	case 0xd4:		/* send byte to AUX */
		if (keyb.enable_aux)
			keyb.command=CMD_WRITEAUX;
		else if (aux_warning++ == 0)
			LOG(LOG_KEYBOARD,LOG_ERROR)("Program is writing 8042 AUX. If you intend to use PS/2 mouse emulation you may consider adding aux=1 to your dosbox.conf");
		break;
	case 0xe0:		/* read test port */
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
	Bit8u status= 0x1c | (keyb.p60changed?0x1:0x0) | (keyb.auxchanged?0x20:0x00);
	return status;
}

void KEYBOARD_AddKey3(KBD_KEYS keytype,bool pressed) {
	Bit8u ret=0,ret2=0;

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
	case KBD_kpminus:ret=0x4e;break;
	case KBD_kp4:ret=0x6b;break;
	case KBD_kp5:ret=0x73;break;
	case KBD_kp6:ret=0x74;break;
	case KBD_kpplus:ret=0x7c;break;
	case KBD_kp1:ret=0x69;break;
	case KBD_kp2:ret=0x72;break;
	case KBD_kp3:ret=0x7a;break;
	case KBD_kp0:ret=0x70;break;
	case KBD_kpperiod:ret=0x71;break;

/*	case KBD_extra_lt_gt:ret=;break; */

	//The Extended keys

	case KBD_kpenter:ret=0x79;break;
	case KBD_rightctrl:ret=0x58;break;
	case KBD_kpdivide:ret=0x4a;break;
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
		E_Exit("Unsupported key press");
		break;
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
	Bit8u ret=0,ret2=0;bool extend=false;

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

/*	case KBD_extra_lt_gt:ret=;break; */

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
		E_Exit("Unsupported key press");
		break;
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

void KEYBOARD_AddKey1(KBD_KEYS keytype,bool pressed) {
	Bit8u ret=0,ret2=0;bool extend=false;

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
		extend=true;
		if (pressed) { ret=0x2a; ret2=0x37; }
		else         { ret=0xb7; ret2=0xaa; }
		return;
	case KBD_lwindows:extend=true;ret=0x5B;break;
	case KBD_rwindows:extend=true;ret=0x5C;break;
	case KBD_rwinmenu:extend=true;ret=0x5D;break;
	case KBD_jp_muhenkan:ret=0x7B;break;
	case KBD_jp_henkan:ret=0x79;break;
	case KBD_jp_hiragana:ret=0x70;break;/*also Katakana */
	default:
		E_Exit("Unsupported key press");
		break;
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
	if (keyb.cb_xlat) {
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
	};
}
	
static void KEYBOARD_ShutDown(Section * sec) {
	TIMER_DelTickHandler(&KEYBOARD_TickHandler);
}

void KEYBOARD_Init(Section* sec) {
	Section_prop *section=static_cast<Section_prop *>(sec);

	sec->AddDestroyFunction(&KEYBOARD_ShutDown);

	if ((keyb.enable_aux=section->Get_bool("aux")) != false) {
		LOG(LOG_KEYBOARD,LOG_NORMAL)("Keyboard AUX emulation enabled");
	}

	allow_keyb_reset = section->Get_bool("allow output port reset");

	keyb.ps2mouse.int33_taken = 0;
	keyb.ps2mouse.reset_mode = MM_STREAM; /* NTS: I was wrong: PS/2 mice default to streaming after reset */

	const char * sbtype=section->Get_string("auxdevice");
	keyb.ps2mouse.type = MOUSE_NONE;
	if (sbtype != NULL) {
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

	IO_RegisterWriteHandler(0x60,write_p60,IO_MB);
	IO_RegisterReadHandler(0x60,read_p60,IO_MB);
	IO_RegisterWriteHandler(0x61,write_p61,IO_MB);
	IO_RegisterReadHandler(0x61,read_p61,IO_MB);
	IO_RegisterWriteHandler(0x64,write_p64,IO_MB);
	IO_RegisterReadHandler(0x64,read_p64,IO_MB);
	TIMER_AddTickHandler(&KEYBOARD_TickHandler);
	write_p61(0,0,0);
	KEYBOARD_Reset();
	AUX_Reset();
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
	keyb.repeat.key=KBD_NONE;
	keyb.repeat.pause=200;
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
void *KEYBOARD_TransferBuffer_PIC_Event = (void*)KEYBOARD_TransferBuffer;
void *KEYBOARD_TickHandler_PIC_Timer = (void*)KEYBOARD_TickHandler;

