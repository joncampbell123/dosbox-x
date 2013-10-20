/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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
 
/* ints/bios.cpp */
void bios_enable_ps2();

/* hardware/keyboard.cpp */
void AUX_INT33_Takeover();
int KEYBOARD_AUX_Active();
void KEYBOARD_AUX_Event(float x,float y,Bitu buttons);

bool en_int33=false;
bool en_bios_ps2mouse=false;

static Bitu call_int33,call_int74,int74_ret_callback,call_mouse_bd;
static Bit16u ps2cbseg,ps2cbofs;
static bool useps2callback,ps2callbackinit;
static Bitu call_ps2;
static RealPt ps2_callback;
static Bit16s oldmouseX, oldmouseY;
// forward
void WriteMouseIntVector(void);

// serial mouse emulation
void on_mouse_event_for_serial(int delta_x,int delta_y,Bit8u buttonstate);

struct button_event {
	Bit8u type;
	Bit8u buttons;
};

#define QUEUE_SIZE 32
#define MOUSE_BUTTONS 3
#define MOUSE_IRQ 12
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
	Bit16u hidden;
	float add_x,add_y;
	Bit16s min_x,max_x,min_y,max_y;
	float mickey_x,mickey_y;
	float x,y;
	button_event event_queue[QUEUE_SIZE];
	Bit8u events;//Increase if QUEUE_SIZE >255 (currently 32)
	Bit16u sub_seg,sub_ofs;
	Bit16u sub_mask;

	bool	background;
	Bit16s	backposx, backposy;
	Bit8u	backData[CURSORX*CURSORY];
	Bit16u*	screenMask;
	Bit16u* cursorMask;
	Bit16s	clipx,clipy;
	Bit16s  hotx,hoty;
	Bit16u  textAndMask, textXorMask;

	float	mickeysPerPixel_x;
	float	mickeysPerPixel_y;
	float	pixelPerMickey_x;
	float	pixelPerMickey_y;
	Bit16u	senv_x_val;
	Bit16u	senv_y_val;
	Bit16u	dspeed_val;
	float	senv_x;
	float	senv_y;
	Bit16u  updateRegion_x[2];
	Bit16u  updateRegion_y[2];
	Bit16u  doubleSpeedThreshold;
	Bit16u  language;
	Bit16u  cursorType;
	Bit16u	oldhidden;
	Bit8u  page;
	bool enabled;
	bool inhibit_draw;
	bool timer_in_progress;
	bool in_UIR;
	Bit8u mode;
	Bit16s gran_x,gran_y;
} mouse;

bool Mouse_SetPS2State(bool use) {
	if (use && (!ps2callbackinit)) {
		useps2callback = false;
		PIC_SetIRQMask(MOUSE_IRQ,true);
		return false;
	}
	useps2callback = use;
	Mouse_AutoLock(useps2callback);
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

void DoPS2Callback(Bit16u data, Bit16s mouseX, Bit16s mouseY) {
	if (useps2callback) {
		Bit16u mdat = (data & 0x03) | 0x08;
		Bit16s xdiff = mouseX-oldmouseX;
		Bit16s ydiff = oldmouseY-mouseY;
		oldmouseX = mouseX;
		oldmouseY = mouseY;
		if ((xdiff>0xff) || (xdiff<-0xff)) mdat |= 0x40;		// x overflow
		if ((ydiff>0xff) || (ydiff<-0xff)) mdat |= 0x80;		// y overflow
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
#define MOUSE_DELAY 5.0

void MOUSE_Limit_Events(Bitu /*val*/) {
	mouse.timer_in_progress = false;
	if (mouse.events) {
		mouse.timer_in_progress = true;
		PIC_AddEvent(MOUSE_Limit_Events,MOUSE_DELAY);
		PIC_ActivateIRQ(MOUSE_IRQ);
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
		PIC_ActivateIRQ(MOUSE_IRQ);
	}
}

// ***************************************************************************
// Mouse cursor - text mode
// ***************************************************************************
/* Write and read directly to the screen. Do no use int_setcursorpos (LOTUS123) */
extern void WriteChar(Bit16u col,Bit16u row,Bit8u page,Bit8u chr,Bit8u attr,bool useattr);
extern void ReadCharAttr(Bit16u col,Bit16u row,Bit8u page,Bit16u * result);

void RestoreCursorBackgroundText() {
	if (mouse.hidden || mouse.inhibit_draw) return;

	if (mouse.background) {
		WriteChar(mouse.backposx,mouse.backposy,real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE),mouse.backData[0],mouse.backData[1],true);
		mouse.background = false;
	}
}

void DrawCursorText() {	
	// Restore Background
	RestoreCursorBackgroundText();


	// Save Background
	mouse.backposx		= POS_X>>3;
	mouse.backposy		= POS_Y>>3;
	if (mouse.mode < 2) mouse.backposx >>= 1; 

	//use current page (CV program)
	Bit8u page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
	Bit16u result;

	ReadCharAttr(mouse.backposx,mouse.backposy,page,&result);
	mouse.backData[0]	= (Bit8u)(result & 0xFF);
	mouse.backData[1]	= (Bit8u)(result>>8);
	mouse.background	= true;
	// Write Cursor
	result = (result & mouse.textAndMask) ^ mouse.textXorMask;
	WriteChar(mouse.backposx,mouse.backposy,page,(Bit8u)(result&0xFF),(Bit8u)(result>>8),true);
}

// ***************************************************************************
// Mouse cursor - graphic mode
// ***************************************************************************

static Bit8u gfxReg3CE[9];
static Bit8u index3C4,gfxReg3C5;
void SaveVgaRegisters() {
	if (IS_VGA_ARCH) {
		for (Bit8u i=0; i<9; i++) {
			IO_Write	(0x3CE,i);
			gfxReg3CE[i] = IO_Read(0x3CF);
		}
		/* Setup some default values in GFX regs that should work */
		IO_Write(0x3CE,3); IO_Write(0x3Cf,0);				//disable rotate and operation
		IO_Write(0x3CE,5); IO_Write(0x3Cf,gfxReg3CE[5]&0xf0);	//Force read/write mode 0

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
	};
	// Clip left
	if (x1<0) {
		addx1 += (-x1);
		x1 = 0;
	};
	// Clip right
	if (x2>mouse.clipx) {
		addx2 = x2 - mouse.clipx;
		x2 = mouse.clipx;
	};
}

void RestoreCursorBackground() {
	if (mouse.hidden || mouse.inhibit_draw) return;

	SaveVgaRegisters();
	if (mouse.background) {
		// Restore background
		Bit16s x,y;
		Bit16u addx1,addx2,addy;
		Bit16u dataPos	= 0;
		Bit16s x1		= mouse.backposx;
		Bit16s y1		= mouse.backposy;
		Bit16s x2		= x1 + CURSORX - 1;
		Bit16s y2		= y1 + CURSORY - 1;	

		ClipCursorArea(x1, x2, y1, y2, addx1, addx2, addy);

		dataPos = addy * CURSORX;
		for (y=y1; y<=y2; y++) {
			dataPos += addx1;
			for (x=x1; x<=x2; x++) {
				INT10_PutPixel(x,y,mouse.page,mouse.backData[dataPos++]);
			};
			dataPos += addx2;
		};
		mouse.background = false;
	};
	RestoreVgaRegisters();
}

void DrawCursor() {
	if (mouse.hidden || mouse.inhibit_draw) return;
	// In Textmode ?
	if (CurMode->type==M_TEXT) {
		DrawCursorText();
		return;
	}

	// Check video page. Seems to be ignored for text mode. 
	// hence the text mode handled above this
	if (real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE)!=mouse.page) return;
// Check if cursor in update region
/*	if ((POS_X >= mouse.updateRegion_x[0]) && (POS_X <= mouse.updateRegion_x[1]) &&
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


	mouse.clipx = (Bit16s)((Bits)CurMode->swidth-1);	/* Get from bios ? */
	mouse.clipy = (Bit16s)((Bits)CurMode->sheight-1);

	/* might be vidmode == 0x13?2:1 */
	Bit16s xratio = 640;
	if (CurMode->swidth>0) xratio/=CurMode->swidth;
	if (xratio==0) xratio = 1;
	
	RestoreCursorBackground();

	SaveVgaRegisters();

	// Save Background
	Bit16s x,y;
	Bit16u addx1,addx2,addy;
	Bit16u dataPos	= 0;
	Bit16s x1		= POS_X / xratio - mouse.hotx;
	Bit16s y1		= POS_Y - mouse.hoty;
	Bit16s x2		= x1 + CURSORX - 1;
	Bit16s y2		= y1 + CURSORY - 1;	

	ClipCursorArea(x1,x2,y1,y2, addx1, addx2, addy);

	dataPos = addy * CURSORX;
	for (y=y1; y<=y2; y++) {
		dataPos += addx1;
		for (x=x1; x<=x2; x++) {
			INT10_GetPixel(x,y,mouse.page,&mouse.backData[dataPos++]);
		};
		dataPos += addx2;
	};
	mouse.background= true;
	mouse.backposx	= POS_X / xratio - mouse.hotx;
	mouse.backposy	= POS_Y - mouse.hoty;

	// Draw Mousecursor
	dataPos = addy * CURSORX;
	for (y=y1; y<=y2; y++) {
		Bit16u scMask = mouse.screenMask[addy+y-y1];
		Bit16u cuMask = mouse.cursorMask[addy+y-y1];
		if (addx1>0) { scMask<<=addx1; cuMask<<=addx1; dataPos += addx1; };
		for (x=x1; x<=x2; x++) {
			Bit8u pixel = 0;
			// ScreenMask
			if (scMask & HIGHESTBIT) pixel = mouse.backData[dataPos];
			scMask<<=1;
			// CursorMask
			if (cuMask & HIGHESTBIT) pixel = pixel ^ 0x0F;
			cuMask<<=1;
			// Set Pixel
			INT10_PutPixel(x,y,mouse.page,pixel);
			dataPos++;
		};
		dataPos += addx2;
	};
	RestoreVgaRegisters();
}

void Mouse_CursorMoved(float xrel,float yrel,float x,float y,bool emulate) {
	INT10_SetCurMode();
	extern bool Mouse_Vertical;
	float dx = xrel * mouse.pixelPerMickey_x;
	float dy = (Mouse_Vertical?-yrel:yrel) * mouse.pixelPerMickey_y;

	if (KEYBOARD_AUX_Active()) {
		KEYBOARD_AUX_Event(xrel,yrel,mouse.buttons);
		return;
	}

	if((fabs(xrel) > 1.0) || (mouse.senv_x < 1.0)) dx *= mouse.senv_x;
	if((fabs(yrel) > 1.0) || (mouse.senv_y < 1.0)) dy *= mouse.senv_y;
	if (useps2callback) dy *= 2;	

	mouse.mickey_x += (dx * mouse.mickeysPerPixel_x);
	mouse.mickey_y += (dy * mouse.mickeysPerPixel_y);
	if (mouse.mickey_x >= 32768.0) mouse.mickey_x -= 65536.0;
	else if (mouse.mickey_x <= -32769.0) mouse.mickey_x += 65536.0;
	if (mouse.mickey_y >= 32768.0) mouse.mickey_y -= 65536.0;
	else if (mouse.mickey_y <= -32769.0) mouse.mickey_y += 65536.0;
	if (emulate) {
		mouse.x += dx;
		mouse.y += dy;
	} else {
		if (CurMode->type == M_TEXT) {
			mouse.x = x*CurMode->swidth;
			mouse.y = y*CurMode->sheight * 8 / CurMode->cheight;
		} else if ((mouse.max_x < 2048) || (mouse.max_y < 2048) || (mouse.max_x != mouse.max_y)) {
			if ((mouse.max_x > 0) && (mouse.max_y > 0)) {
				mouse.x = x*mouse.max_x;
				mouse.y = y*mouse.max_y;
			} else {
				mouse.x += xrel;
				mouse.y += yrel;
			}
		} else { // Games faking relative movement through absolute coordinates. Quite surprising that this actually works..
			mouse.x += xrel;
			mouse.y += yrel;
		}
	}

	/* serial mouse, if connected, also wants to know about it */
	on_mouse_event_for_serial((int)(dx),(int)(dy),mouse.buttons);

	/* ignore constraints if using PS2 mouse callback in the bios */

	if (!useps2callback) {		
		if (mouse.x > mouse.max_x) mouse.x = mouse.max_x;
		if (mouse.x < mouse.min_x) mouse.x = mouse.min_x;
		if (mouse.y > mouse.max_y) mouse.y = mouse.max_y;
		if (mouse.y < mouse.min_y) mouse.y = mouse.min_y;
	} else {
		if (mouse.x >= 32768.0) mouse.x -= 65536.0;
		else if (mouse.x <= -32769.0) mouse.x += 65536.0;
		if (mouse.y >= 32768.0) mouse.y -= 65536.0;
		else if (mouse.y <= -32769.0) mouse.y += 65536.0;
	}
	Mouse_AddEvent(MOUSE_HAS_MOVED);
	DrawCursor();
}

void Mouse_CursorSet(float x,float y) {
	mouse.x=x;
	mouse.y=y;
	DrawCursor();
}

void Mouse_ButtonPressed(Bit8u button) {
	if (KEYBOARD_AUX_Active()) {
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

		KEYBOARD_AUX_Event(0,0,mouse.buttons);
		return;
	}

	switch (button) {
#if (MOUSE_BUTTONS >= 1)
	case 0:
		mouse.buttons|=1;
		Mouse_AddEvent(MOUSE_LEFT_PRESSED);
		break;
#endif
#if (MOUSE_BUTTONS >= 2)
	case 1:
		mouse.buttons|=2;
		Mouse_AddEvent(MOUSE_RIGHT_PRESSED);
		break;
#endif
#if (MOUSE_BUTTONS >= 3)
	case 2:
		mouse.buttons|=4;
		Mouse_AddEvent(MOUSE_MIDDLE_PRESSED);
		break;
#endif
	default:
		return;
	}
	mouse.times_pressed[button]++;
	mouse.last_pressed_x[button]=POS_X;
	mouse.last_pressed_y[button]=POS_Y;

	/* serial mouse, if connected, also wants to know about it */
	on_mouse_event_for_serial(0,0,mouse.buttons);
}

void Mouse_ButtonReleased(Bit8u button) {
	if (KEYBOARD_AUX_Active()) {
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
			default:
				return;
		}

		KEYBOARD_AUX_Event(0,0,mouse.buttons);
		return;
	}

	switch (button) {
#if (MOUSE_BUTTONS >= 1)
	case 0:
		mouse.buttons&=~1;
		Mouse_AddEvent(MOUSE_LEFT_RELEASED);
		break;
#endif
#if (MOUSE_BUTTONS >= 2)
	case 1:
		mouse.buttons&=~2;
		Mouse_AddEvent(MOUSE_RIGHT_RELEASED);
		break;
#endif
#if (MOUSE_BUTTONS >= 3)
	case 2:
		mouse.buttons&=~4;
		Mouse_AddEvent(MOUSE_MIDDLE_RELEASED);
		break;
#endif
	default:
		return;
	}
	mouse.times_released[button]++;	
	mouse.last_released_x[button]=POS_X;
	mouse.last_released_y[button]=POS_Y;

	/* serial mouse, if connected, also wants to know about it */
	on_mouse_event_for_serial(0,0,mouse.buttons);
}

static void Mouse_SetMickeyPixelRate(Bit16s px, Bit16s py){
	if ((px!=0) && (py!=0)) {
		mouse.mickeysPerPixel_x	 = (float)px/X_MICKEY;
		mouse.mickeysPerPixel_y  = (float)py/Y_MICKEY;
		mouse.pixelPerMickey_x	 = X_MICKEY/(float)px;
		mouse.pixelPerMickey_y 	 = Y_MICKEY/(float)py;	
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
	PIC_SetIRQMask(MOUSE_IRQ,false);
}

//Does way to much. Many things should be moved to mouse reset one day
void Mouse_NewVideoMode(void) {
	mouse.inhibit_draw = false;
	/* Get the correct resolution from the current video mode */
	Bit8u mode = mem_readb(BIOS_VIDEO_MODE);
	if(mode == mouse.mode) {LOG(LOG_MOUSE,LOG_NORMAL)("New video is the same as the old"); /*return;*/}
	mouse.gran_x = (Bit16s)0xffff;
	mouse.gran_y = (Bit16s)0xffff;
	switch (mode) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x07: {
		mouse.gran_x = (mode<2)?0xfff0:0xfff8;
		mouse.gran_y = (Bit16s)0xfff8;
		Bitu rows = real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS);
		if ((rows == 0) || (rows > 250)) rows = 25 - 1;
		mouse.max_y = 8*(rows+1) - 1;
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
		break;
	case 0x0f:
	case 0x10:
		mouse.max_y = 349;
		break;
	case 0x11:
	case 0x12:
		mouse.max_y = 479;
		break;
	default:
		LOG(LOG_MOUSE,LOG_ERROR)("Unhandled videomode %X on reset",mode);
		mouse.inhibit_draw = true;
		return;
	}
	mouse.mode = mode;
	mouse.hidden = 1;
	mouse.max_x = 639;
	mouse.min_x = 0;
	mouse.min_y = 0;

	mouse.events = 0;
	mouse.timer_in_progress = false;
	PIC_RemoveEvents(MOUSE_Limit_Events);

	mouse.hotx		 = 0;
	mouse.hoty		 = 0;
	mouse.background = false;
	mouse.screenMask = defaultScreenMask;
	mouse.cursorMask = defaultCursorMask;
	mouse.textAndMask= defaultTextAndMask;
	mouse.textXorMask= defaultTextXorMask;
	mouse.language   = 0;
	mouse.page               = 0;
	mouse.doubleSpeedThreshold = 64;
	mouse.updateRegion_x[0] = 1;
	mouse.updateRegion_y[0] = 1;
	mouse.updateRegion_x[1] = 1;
	mouse.updateRegion_y[1] = 1;
	mouse.cursorType = 0;
	mouse.enabled=true;
	mouse.oldhidden=1;

	oldmouseX = static_cast<Bit16s>(mouse.x);
	oldmouseY = static_cast<Bit16s>(mouse.y);


}

//Much too empty, Mouse_NewVideoMode contains stuff that should be in here
static void Mouse_Reset(void) {
	/* Remove drawn mouse Legends of Valor */
	if (CurMode->type!=M_TEXT) RestoreCursorBackground();
	else RestoreCursorBackgroundText();
	mouse.hidden = 1;

	Mouse_NewVideoMode();
	Mouse_SetMickeyPixelRate(8,16);

	mouse.mickey_x = 0;
	mouse.mickey_y = 0;

	// Dont set max coordinates here. it is done by SetResolution!
	mouse.x = static_cast<float>((mouse.max_x + 1)/ 2);
	mouse.y = static_cast<float>((mouse.max_y + 1)/ 2);
	mouse.sub_mask = 0;
	mouse.in_UIR = false;
}

static Bitu INT33_Handler(void) {
//	LOG(LOG_MOUSE,LOG_NORMAL)("MOUSE: %04X %X %X %d %d",reg_ax,reg_bx,reg_cx,POS_X,POS_Y);
	INT10_SetCurMode();
	switch (reg_ax) {
	case 0x00:	/* Reset Driver and Read Status */
		Mouse_ResetHardware(); /* fallthrough */
	case 0x21:	/* Software Reset */
		extern bool Mouse_Drv;
		if (Mouse_Drv) {
			reg_ax=0xffff;
			reg_bx=MOUSE_BUTTONS;
			Mouse_Reset();
			Mouse_AutoLock(true);
			AUX_INT33_Takeover();
			LOG(LOG_KEYBOARD,LOG_NORMAL)("INT 33h reset");
		}
		break;
	case 0x01:	/* Show Mouse */
		if(mouse.hidden) mouse.hidden--;
		Mouse_AutoLock(true);
		DrawCursor();
		break;
	case 0x02:	/* Hide Mouse */
		{
			if (CurMode->type!=M_TEXT) RestoreCursorBackground();
			else RestoreCursorBackgroundText();
			mouse.hidden++;
		}
		break;
	case 0x03:	/* Return position and Button Status */
		reg_bx=mouse.buttons;
		reg_cx=POS_X;
		reg_dx=POS_Y;
		break;
	case 0x04:	/* Position Mouse */
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
		break;
	case 0x05:	/* Return Button Press Data */
		{
			Bit16u but=reg_bx;
			reg_ax=mouse.buttons;
			if (but>=MOUSE_BUTTONS) but = MOUSE_BUTTONS - 1;
			reg_cx=mouse.last_pressed_x[but];
			reg_dx=mouse.last_pressed_y[but];
			reg_bx=mouse.times_pressed[but];
			mouse.times_pressed[but]=0;
			break;
		}
	case 0x06:	/* Return Button Release Data */
		{
			Bit16u but=reg_bx;
			reg_ax=mouse.buttons;
			if (but>=MOUSE_BUTTONS) but = MOUSE_BUTTONS - 1;
			reg_cx=mouse.last_released_x[but];
			reg_dx=mouse.last_released_y[but];
			reg_bx=mouse.times_released[but];
			mouse.times_released[but]=0;
			break;
		}
	case 0x07:	/* Define horizontal cursor range */
		{	//lemmings set 1-640 and wants that. iron seeds set 0-640 but doesn't like 640
			//Iron seed works if newvideo mode with mode 13 sets 0-639
			//Larry 6 actually wants newvideo mode with mode 13 to set it to 0-319
			Bit16s max,min;
			if ((Bit16s)reg_cx<(Bit16s)reg_dx) { min=(Bit16s)reg_cx;max=(Bit16s)reg_dx;}
			else { min=(Bit16s)reg_dx;max=(Bit16s)reg_cx;}
			mouse.min_x=min;
			mouse.max_x=max;
			/* Battlechess wants this */
			if(mouse.x > mouse.max_x) mouse.x = mouse.max_x;
			if(mouse.x < mouse.min_x) mouse.x = mouse.min_x;
			/* Or alternatively this: 
			mouse.x = (mouse.max_x - mouse.min_x + 1)/2;*/
			LOG(LOG_MOUSE,LOG_NORMAL)("Define Hortizontal range min:%d max:%d",min,max);
		}
		break;
	case 0x08:	/* Define vertical cursor range */
		{	// not sure what to take instead of the CurMode (see case 0x07 as well)
			// especially the cases where sheight= 400 and we set it with the mouse_reset to 200
			//disabled it at the moment. Seems to break syndicate who want 400 in mode 13
			Bit16s max,min;
			if ((Bit16s)reg_cx<(Bit16s)reg_dx) { min=(Bit16s)reg_cx;max=(Bit16s)reg_dx;}
			else { min=(Bit16s)reg_dx;max=(Bit16s)reg_cx;}
			mouse.min_y=min;
			mouse.max_y=max;
			/* Battlechess wants this */
			if(mouse.y > mouse.max_y) mouse.y = mouse.max_y;
			if(mouse.y < mouse.min_y) mouse.y = mouse.min_y;
			/* Or alternatively this: 
			mouse.y = (mouse.max_y - mouse.min_y + 1)/2;*/
			LOG(LOG_MOUSE,LOG_NORMAL)("Define Vertical range min:%d max:%d",min,max);
		}
		break;
	case 0x09:	/* Define GFX Cursor */
		{
			PhysPt src = SegPhys(es)+reg_dx;
			MEM_BlockRead(src          ,userdefScreenMask,CURSORY*2);
			MEM_BlockRead(src+CURSORY*2,userdefCursorMask,CURSORY*2);
			mouse.screenMask = userdefScreenMask;
			mouse.cursorMask = userdefCursorMask;
			mouse.hotx		 = reg_bx;
			mouse.hoty		 = reg_cx;
			mouse.cursorType = 2;
			DrawCursor();
		}
		break;
	case 0x0a:	/* Define Text Cursor */
		mouse.cursorType = reg_bx;
		mouse.textAndMask = reg_cx;
		mouse.textXorMask = reg_dx;
		break;
	case 0x0b:	/* Read Motion Data */
		reg_cx=static_cast<Bit16s>(mouse.mickey_x);
		reg_dx=static_cast<Bit16s>(mouse.mickey_y);
		mouse.mickey_x=0;
		mouse.mickey_y=0;
		break;
	case 0x0c:	/* Define interrupt subroutine parameters */
		mouse.sub_mask=reg_cx;
		mouse.sub_seg=SegValue(es);
		mouse.sub_ofs=reg_dx;
		Mouse_AutoLock(true); //Some games don't seem to reset the mouse before using
		break;
	case 0x0f:	/* Define mickey/pixel rate */
		Mouse_SetMickeyPixelRate(reg_cx,reg_dx);
		break;
	case 0x10:      /* Define screen region for updating */
		mouse.updateRegion_x[0]=reg_cx;
		mouse.updateRegion_y[0]=reg_dx;
		mouse.updateRegion_x[1]=reg_si;
		mouse.updateRegion_y[1]=reg_di;
		break;
	case 0x11:      /* Get number of buttons */
		reg_ax=0xffff;
		reg_bx=MOUSE_BUTTONS;
		break;
	case 0x13:      /* Set double-speed threshold */
		mouse.doubleSpeedThreshold=(reg_bx ? reg_bx : 64);
 		break;
	case 0x14: /* Exchange event-handler */ 
		{	
			Bit16u oldSeg = mouse.sub_seg;
			Bit16u oldOfs = mouse.sub_ofs;
			Bit16u oldMask= mouse.sub_mask;
			// Set new values
			mouse.sub_mask= reg_cx;
			mouse.sub_seg = SegValue(es);
			mouse.sub_ofs = reg_dx;
			// Return old values
			reg_cx = oldMask;
			reg_dx = oldOfs;
			SegSet16(es,oldSeg);
		}
		break;		
	case 0x15: /* Get Driver storage space requirements */
		reg_bx = sizeof(mouse);
		break;
	case 0x16: /* Save driver state */
		{
			LOG(LOG_MOUSE,LOG_WARN)("Saving driver state...");
			PhysPt dest = SegPhys(es)+reg_dx;
			MEM_BlockWrite(dest, &mouse, sizeof(mouse));
		}
		break;
	case 0x17: /* load driver state */
		{
			LOG(LOG_MOUSE,LOG_WARN)("Loading driver state...");
			PhysPt src = SegPhys(es)+reg_dx;
			MEM_BlockRead(src, &mouse, sizeof(mouse));
		}
		break;
	case 0x1a:	/* Set mouse sensitivity */
		// ToDo : double mouse speed value
		Mouse_SetSensitivity(reg_bx,reg_cx,reg_dx);

		LOG(LOG_MOUSE,LOG_WARN)("Set sensitivity used with %d %d (%d)",reg_bx,reg_cx,reg_dx);
		break;
	case 0x1b:	/* Get mouse sensitivity */
		reg_bx = mouse.senv_x_val;
		reg_cx = mouse.senv_y_val;
		reg_dx = mouse.dspeed_val;

		LOG(LOG_MOUSE,LOG_WARN)("Get sensitivity %d %d",reg_bx,reg_cx);
		break;
	case 0x1c:	/* Set interrupt rate */
		/* Can't really set a rate this is host determined */
		break;
	case 0x1d:      /* Set display page number */
		mouse.page=reg_bl;
		break;
	case 0x1e:      /* Get display page number */
		reg_bx=mouse.page;
		break;
	case 0x1f:	/* Disable Mousedriver */
		/* ES:BX old mouse driver Zero at the moment TODO */ 
		reg_bx=0;
		SegSet16(es,0);	   
		mouse.enabled=false; /* Just for reporting not doing a thing with it */
		mouse.oldhidden=mouse.hidden;
		mouse.hidden=1;
		break;
	case 0x20:	/* Enable Mousedriver */
		mouse.enabled=true;
		mouse.hidden=mouse.oldhidden;
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
		mouse.language=reg_bx;
		break;
	case 0x23:      /* Get language for messages */
		reg_bx=mouse.language;
		break;
	case 0x24:	/* Get Software version and mouse type */
		reg_bx=0x805;	//Version 8.05 woohoo 
		reg_ch=0x04;	/* PS/2 type */
		reg_cl=0;		/* PS/2 (unused) */
		break;
	case 0x26: /* Get Maximum virtual coordinates */
		reg_bx=(mouse.enabled ? 0x0000 : 0xffff);
		reg_cx=(Bit16u)mouse.max_x;
		reg_dx=(Bit16u)mouse.max_y;
		break;
	case 0x2a:	/* Get cursor hot spot */
		reg_al=(Bit8u)-mouse.hidden;	// Microsoft uses a negative byte counter for cursor visibility
		reg_bx=(Bit16u)mouse.hotx;
		reg_cx=(Bit16u)mouse.hoty;
		reg_dx=0x04;	// PS/2 mouse type
		break;
	case 0x31: /* Get Current Minimum/Maximum virtual coordinates */
		reg_ax=(Bit16u)mouse.min_x;
		reg_bx=(Bit16u)mouse.min_y;
		reg_cx=(Bit16u)mouse.max_x;
		reg_dx=(Bit16u)mouse.max_y;
		break;
	default:
		LOG(LOG_MOUSE,LOG_ERROR)("Mouse Function %04X not implemented!",reg_ax);
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
//	LOG_MSG("MOUSE BD: %04X %X %X %X %d %d",reg_ax,reg_bx,reg_cx,reg_dx,POS_X,POS_Y);
	
	// some functions are treated in a special way (additional registers)
	switch (rax) {
		case 0x09:	/* Define GFX Cursor */
		case 0x16:	/* Save driver state */
		case 0x17:	/* load driver state */
			SegSet16(es,SegValue(ds));
			break;
		case 0x0c:	/* Define interrupt subroutine parameters */
		case 0x14:	/* Exchange event-handler */ 
			if (reg_bx!=0) SegSet16(es,reg_bx);
			else SegSet16(es,SegValue(ds));
			break;
		case 0x10:	/* Define screen region for updating */
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
		case 0x1f:	/* Disable Mousedriver */
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
	if (mouse.events>0) {
		mouse.events--;
		/* Check for an active Interrupt Handler that will get called */
		if (mouse.sub_mask & mouse.event_queue[mouse.events].type) {
			reg_ax=mouse.event_queue[mouse.events].type;
			reg_bx=mouse.event_queue[mouse.events].buttons;
			reg_cx=POS_X;
			reg_dx=POS_Y;
			reg_si=static_cast<Bit16s>(mouse.mickey_x);
			reg_di=static_cast<Bit16s>(mouse.mickey_y);
			CPU_Push16(RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
			CPU_Push16(RealOff(CALLBACK_RealPointer(int74_ret_callback)));
			SegSet16(cs, mouse.sub_seg);
			reg_ip = mouse.sub_ofs;
			if(mouse.in_UIR) LOG(LOG_MOUSE,LOG_ERROR)("Already in UIR!");
			mouse.in_UIR = true;
		} else if (useps2callback) {
			CPU_Push16(RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
			CPU_Push16(RealOff(CALLBACK_RealPointer(int74_ret_callback)));
			DoPS2Callback(mouse.event_queue[mouse.events].buttons, static_cast<Bit16s>(mouse.x), static_cast<Bit16s>(mouse.y));
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

Bitu MOUSE_UserInt_CB_Handler(void) {
	mouse.in_UIR = false;
	if (mouse.events) {
		if (!mouse.timer_in_progress) {
			mouse.timer_in_progress = true;
			PIC_AddEvent(MOUSE_Limit_Events,MOUSE_DELAY);
		}
	}
	return CBRET_NONE;
}

void MOUSE_Init(Section* sec) {
	Section_prop *section=static_cast<Section_prop *>(sec);
	RealPt i33loc=0;

	if (en_int33=section->Get_bool("int33")) {
		LOG(LOG_KEYBOARD,LOG_NORMAL)("INT 33H emulation enabled");
	}

	if (en_bios_ps2mouse=section->Get_bool("biosps2")) {
		LOG(LOG_KEYBOARD,LOG_NORMAL)("INT 15H PS/2 emulation enabled");
		bios_enable_ps2();
	}

	if (en_int33) {
		// Callback for mouse interrupt 0x33
		call_int33=CALLBACK_Allocate();
		// i33loc=RealMake(CB_SEG+1,(call_int33*CB_SIZE)-0x10);
		i33loc=RealMake(DOS_GetMemory(0x1)-1,0x10);
		CALLBACK_Setup(call_int33,&INT33_Handler,CB_MOUSE,Real2Phys(i33loc),"Mouse");
		// Wasteland needs low(seg(int33))!=0 and low(ofs(int33))!=0
		real_writed(0,0x33<<2,i33loc);
	}
	else {
		call_int33=0;
	}

	call_mouse_bd=CALLBACK_Allocate();
	CALLBACK_Setup(call_mouse_bd,&MOUSE_BD_Handler,CB_RETF8,
		PhysMake(RealSeg(i33loc),RealOff(i33loc)+2),"MouseBD");
	// pseudocode for CB_MOUSE (including the special backdoor entry point):
	//	jump near i33hd
	//	callback MOUSE_BD_Handler
	//	retf 8
	//  label i33hd:
	//	callback INT33_Handler
	//	iret


	// Callback for ps2 irq
	call_int74=CALLBACK_Allocate();
	CALLBACK_Setup(call_int74,&INT74_Handler,CB_IRQ12,"int 74");
	// pseudocode for CB_IRQ12:
	//	push ds
	//	push es
	//	pushad
	//	sti
	//	callback INT74_Handler
	//		doesn't return here, but rather to CB_IRQ12_RET
	//		(ps2 callback/user callback inbetween if requested)

	int74_ret_callback=CALLBACK_Allocate();
	CALLBACK_Setup(int74_ret_callback,&MOUSE_UserInt_CB_Handler,CB_IRQ12_RET,"int 74 ret");
	// pseudocode for CB_IRQ12_RET:
	//	callback MOUSE_UserInt_CB_Handler
	//	cli
	//	mov al, 0x20
	//	out 0xa0, al
	//	out 0x20, al
	//	popad
	//	pop es
	//	pop ds
	//	iret

	Bit8u hwvec=(MOUSE_IRQ>7)?(0x70+MOUSE_IRQ-8):(0x8+MOUSE_IRQ);
	RealSetVec(hwvec,CALLBACK_RealPointer(call_int74));

	// Callback for ps2 user callback handling
	useps2callback = false; ps2callbackinit = false;
 	call_ps2=CALLBACK_Allocate();
	CALLBACK_Setup(call_ps2,&PS2_Handler,CB_RETF,"ps2 bios callback");
	ps2_callback=CALLBACK_RealPointer(call_ps2);

	memset(&mouse,0,sizeof(mouse));
	mouse.hidden = 1; //Hide mouse on startup
	mouse.timer_in_progress = false;
	mouse.mode = 0xFF; //Non existing mode

   	mouse.sub_mask=0;
	mouse.sub_seg=0x6362;	// magic value
	mouse.sub_ofs=0;

	Mouse_ResetHardware();
	Mouse_Reset();
	Mouse_SetSensitivity(50,50,50);
}

void *MOUSE_Limit_Events_PIC_Event = (void*)MOUSE_Limit_Events;

