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
 */


#include "dos_inc.h"
#include "../ints/int10.h"
#include <string.h>
#include "inout.h"
#include "shiftjis.h"
#include "callback.h"

#define NUMBER_ANSI_DATA 10

extern bool inshell;
extern bool DOS_BreakFlag;
extern bool DOS_BreakConioFlag;
extern unsigned char pc98_function_row_mode;

Bitu INT10_Handler(void);
Bitu INT16_Handler_Wrap(void);

bool inhibited_ControlFn(void);
void pc98_function_row_user_toggle(void);
void update_pc98_function_row(unsigned char setting,bool force_redraw=false);
void PC98_GetFuncKeyEscape(size_t &len,unsigned char buf[16],const unsigned int i);
void PC98_GetShiftFuncKeyEscape(size_t &len,unsigned char buf[16],const unsigned int i);
void PC98_GetEditorKeyEscape(size_t &len,unsigned char buf[16],const unsigned int scan);
void PC98_GetVFuncKeyEscape(size_t &len,unsigned char buf[16],const unsigned int i);
void PC98_GetShiftVFuncKeyEscape(size_t &len,unsigned char buf[16],const unsigned int i);
void PC98_GetCtrlFuncKeyEscape(size_t &len,unsigned char buf[16],const unsigned int i);
void PC98_GetCtrlVFuncKeyEscape(size_t &len,unsigned char buf[16],const unsigned int i);

ShiftJISDecoder con_sjis;

uint16_t last_int16_code = 0;

static size_t dev_con_pos=0,dev_con_max=0;
static unsigned char dev_con_readbuf[64];

uint8_t DefaultANSIAttr() {
	return IS_PC98_ARCH ? 0xE1 : 0x07;
}

class device_CON : public DOS_Device {
public:
	device_CON();
	bool Read(uint8_t * data,uint16_t * size);
	bool Write(const uint8_t * data,uint16_t * size);
	bool Seek(uint32_t * pos,uint32_t type);
	bool Close();
	uint16_t GetInformation(void);
	bool ReadFromControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode) { (void)bufptr; (void)size; (void)retcode; return false; }
	bool WriteToControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode) { (void)bufptr; (void)size; (void)retcode; return false; }
    bool ANSI_SYS_installed();
private:
	void ClearAnsi(void);
	void Output(uint8_t chr);
	uint8_t readcache;
	struct ansi { /* should create a constructor, which would fill them with the appropriate values */
        bool installed;     // ANSI.SYS is installed (and therefore escapes are handled)
		bool esc;
		bool sci;
        bool equcurp;       // ????? ESC = Y X      cursor pos    (not sure if PC-98 specific or general to DOS ANSI.SYS)
        bool pc98rab;       // PC-98 ESC [ > ...    (right angle bracket) I will rename this variable if MS-DOS ANSI.SYS also supports this sequence
		bool enabled;
		uint8_t attr;         // machine-specific
		uint8_t data[NUMBER_ANSI_DATA];
		uint8_t numberofarg;
		uint16_t nrows;
		uint16_t ncols;
		uint8_t savecol;
		uint8_t saverow;
		bool warned;
	} ansi;

    // ESC M
    void ESC_M(void) {
        LineFeedRev();
        ClearAnsi();
    }

    // ESC D
    void ESC_D(void) {
        LineFeed();
        ClearAnsi();
    }

    // ESC E
    void ESC_E(void) {
        Real_INT10_TeletypeOutputAttr('\n',ansi.attr,ansi.enabled);
        ClearAnsi();
    }

    // ESC [ A
    void ESC_BRACKET_A(void) {
        uint8_t page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
        uint8_t tempdata;
        uint8_t col,row;

        col=CURSOR_POS_COL(page) ;
        row=CURSOR_POS_ROW(page) ;
        tempdata = (ansi.data[0]? ansi.data[0] : 1);
        if(tempdata > row) { row=0; } 
        else { row-=tempdata;}
        Real_INT10_SetCursorPos(row,col,page);
        ClearAnsi();
    }

    // ESC [ B
    void ESC_BRACKET_B(void) {
        uint8_t page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
        uint8_t tempdata;
        uint8_t col,row;

        col=CURSOR_POS_COL(page) ;
        row=CURSOR_POS_ROW(page) ;
        if (!IS_PC98_ARCH)
            ansi.nrows = real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS) + 1;
        tempdata = (ansi.data[0]? ansi.data[0] : 1);
        if(tempdata + static_cast<Bitu>(row) >= ansi.nrows)
        { row = ansi.nrows - 1;}
        else	{ row += tempdata; }
        Real_INT10_SetCursorPos(row,col,page);
        ClearAnsi();
    }

    // ESC [ C
    void ESC_BRACKET_C(void) {
        uint8_t page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
        uint8_t tempdata;
        uint8_t col,row;

        col=CURSOR_POS_COL(page);
        row=CURSOR_POS_ROW(page);
        if (!IS_PC98_ARCH)
            ansi.ncols = real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
        tempdata=(ansi.data[0]? ansi.data[0] : 1);
        if(tempdata + static_cast<Bitu>(col) >= ansi.ncols) 
        { col = ansi.ncols - 1;} 
        else	{ col += tempdata;}
        Real_INT10_SetCursorPos(row,col,page);
        ClearAnsi();
    }

    // ESC [ D
    void ESC_BRACKET_D(void) {
        uint8_t page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
        uint8_t tempdata;
        uint8_t col,row;

        col=CURSOR_POS_COL(page);
        row=CURSOR_POS_ROW(page);
        tempdata=(ansi.data[0]? ansi.data[0] : 1);
        if(tempdata > col) {col = 0;}
        else { col -= tempdata;}
        Real_INT10_SetCursorPos(row,col,page);
        ClearAnsi();
    }

    // ESC = Y X
    void ESC_EQU_cursor_pos(void) {
        uint8_t page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);

        /* This is what the PC-98 ANSI driver does */
        if(ansi.data[0] >= 0x20) ansi.data[0] -= 0x20;
        else ansi.data[0] = 0;
        if(ansi.data[1] >= 0x20) ansi.data[1] -= 0x20;
        else ansi.data[1] = 0;

        if (!IS_PC98_ARCH) {
            ansi.ncols = real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
            ansi.nrows = real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS) + 1;
        }
        /* Turn them into positions that are on the screen */
        if(ansi.data[0] >= ansi.nrows) ansi.data[0] = (uint8_t)ansi.nrows - 1;
        if(ansi.data[1] >= ansi.ncols) ansi.data[1] = (uint8_t)ansi.ncols - 1;
        Real_INT10_SetCursorPos(ansi.data[0],ansi.data[1],page);

        ClearAnsi();
    }

	static void Real_INT10_SetCursorPos(uint8_t row,uint8_t col,uint8_t page) {
		uint16_t		oldax,oldbx,olddx;

		oldax=reg_ax;
		oldbx=reg_bx;
		olddx=reg_dx;

		reg_ah=0x2;
		reg_dh=row;
		reg_dl=col;
		reg_bh=page;

        /* FIXME: PC-98 emulation should eventually use CONIO emulation that
         *        better emulates the actual platform. The purpose of this
         *        hack is to allow our code to call into INT 10h without
         *        setting up an INT 10h vector */
        if (IS_PC98_ARCH)
            INT10_Handler();
        else
            CALLBACK_RunRealInt(0x10);

		reg_ax=oldax;
		reg_bx=oldbx;
		reg_dx=olddx;
	}

    /* Common function to turn specific scan codes into ANSI codes.
     * This is a separate function so that both Read() and GetInformation() can use it.
     * GetInformation needs to handle the scan code on entry in order to correctly
     * assert whether Read() will return data or not. Some scan codes are ignored by
     * the CON driver, therefore even though the BIOS says there is key data, Read()
     * will not return anything and will block. */
    bool CommonPC98ExtScanConversionToReadBuf(unsigned char code) {
        size_t esclen;

        switch (code) {
            case 0x36: // ROLL UP
            case 0x37: // ROLL DOWN
            case 0x38: // INS
            case 0x39: // DEL
            case 0x3A: // UP ARROW
            case 0x3B: // LEFT ARROW
            case 0x3C: // RIGHT ARROW
            case 0x3D: // DOWN ARROW
            case 0x3E: // HOME/CLR
            case 0x3F: // HELP
            case 0x40: // KEYPAD -
                PC98_GetEditorKeyEscape(/*&*/esclen,dev_con_readbuf,code); dev_con_pos=0; dev_con_max=esclen;
                return (dev_con_max != 0)?true:false;
            case 0x52: // VF1
            case 0x53: // VF2
            case 0x54: // VF3
            case 0x55: // VF4
            case 0x56: // VF5
                PC98_GetVFuncKeyEscape(/*&*/esclen,dev_con_readbuf,code+1u-0x52u); dev_con_pos=0; dev_con_max=esclen;
                return (dev_con_max != 0)?true:false;
            case 0x62: // F1
            case 0x63: // F2
            case 0x64: // F3
            case 0x65: // F4
            case 0x66: // F5
            case 0x67: // F6
            case 0x68: // F7
            case 0x69: // F8
            case 0x6A: // F9
            case 0x6B: // F10
                PC98_GetFuncKeyEscape(/*&*/esclen,dev_con_readbuf,code+1u-0x62u); dev_con_pos=0; dev_con_max=esclen;
                return (dev_con_max != 0)?true:false;
            case 0x82: // Shift+F1
            case 0x83: // Shift+F2
            case 0x84: // Shift+F3
            case 0x85: // Shift+F4
            case 0x86: // Shift+F5
            case 0x87: // Shift+F6
            case 0x88: // Shift+F7
            case 0x89: // Shift+F8
            case 0x8A: // Shift+F9
            case 0x8B: // Shift+F10
                PC98_GetShiftFuncKeyEscape(/*&*/esclen,dev_con_readbuf,code+1u-0x82u); dev_con_pos=0; dev_con_max=esclen;
                return (dev_con_max != 0)?true:false;
            case 0x92: // Control+F1
            case 0x93: // Control+F2
            case 0x94: // Control+F3
            case 0x95: // Control+F4
            case 0x96: // Control+F5
            case 0x97: // Control+F6
            case 0x98: // Control+F7
            case 0x99: // Control+F8
            case 0x9A: // Control+F9
            case 0x9B: // Control+F10
                if (inhibited_ControlFn()) {
                    PC98_GetCtrlFuncKeyEscape(/*&*/esclen,dev_con_readbuf,code+1u-0x92u); dev_con_pos=0; dev_con_max=esclen;
                    return (dev_con_max != 0)?true:false;
                }
                else if (code == 0x97) {// CTRL+F6   Toggle 20/25-line text      HANDLED INTERNALLY, NEVER RETURNED TO CONSOLE
                    /* toggle the bit and change the text layer */
                    {
                        uint8_t b = real_readb(0x60,0x113);
                        real_writeb(0x60,0x113,b ^ 1);

                        reg_ah = 0x0A; /* Set CRT mode */
                        reg_al = b;
                        CALLBACK_RunRealInt(0x18);

                        reg_ah = 0x11; /* show cursor (Func 0x0A hides the cursor) */
                        CALLBACK_RunRealInt(0x18);
                    }

                    /* clear the screen */
                    {
                        uint8_t page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);

                        /* it also redraws the function key row */
                        update_pc98_function_row(pc98_function_row_mode,true);

                        INT10_ScrollWindow(0,0,255,255,0,ansi.attr,page);
                        Real_INT10_SetCursorPos(0,0,page);
                    }
                }
                else if (code == 0x98) {// CTRL+F7   Toggle function key row     HANDLED INTERNALLY, NEVER RETURNED TO CONSOLE
                    void pc98_function_row_user_toggle(void);
                    pc98_function_row_user_toggle();
                }
                else if (code == 0x99) {// CTRL+F8   Clear screen, home cursor   HANDLED INTERNALLY, NEVER RETURNED TO CONSOLE
                    uint8_t page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);

                    /* it also redraws the function key row */
                    update_pc98_function_row(pc98_function_row_mode,true);

                    INT10_ScrollWindow(0,0,255,255,0,ansi.attr,page);
                    Real_INT10_SetCursorPos(0,0,page);
                    ClearAnsi();
                }
                break;

            case 0xC2: // VF1
            case 0xC3: // VF2
            case 0xC4: // VF3
            case 0xC5: // VF4
            case 0xC6: // VF5
                PC98_GetShiftVFuncKeyEscape(/*&*/esclen,dev_con_readbuf,code+1u-0xC2u); dev_con_pos=0; dev_con_max=esclen;
                return (dev_con_max != 0)?true:false;

            case 0xD2: // VF1
            case 0xD3: // VF2
            case 0xD4: // VF3
            case 0xD5: // VF4
            case 0xD6: // VF5
                if (inhibited_ControlFn()) {
                    PC98_GetCtrlVFuncKeyEscape(/*&*/esclen,dev_con_readbuf,code+1u-0xD2u); dev_con_pos=0; dev_con_max=esclen;
                    return (dev_con_max != 0)?true:false;
                }
                break;
#if 0
                // ROLL UP  --          --          --
                // POLL DOWN--          --          --
                // COPY     --          --          --
                // HOME/CLR 0x1A        0x1E        --
                // HELP     --          --          --
#endif
        }

        return false;
    }

	static void Real_INT10_TeletypeOutput(uint8_t xChar,uint8_t xAttr) {
        if (IS_PC98_ARCH) {
            if (con_sjis.take(xChar)) {
                BIOS_NCOLS;
                uint8_t page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
                uint8_t cur_row=CURSOR_POS_ROW(page);
                uint8_t cur_col=CURSOR_POS_COL(page);
                unsigned char cw = con_sjis.doublewide ? 2 : 1;

                /* FIXME: I'm not sure what NEC's ANSI driver does if a doublewide character is printed at column 79 */
                if ((cur_col+cw) > ncols) {
                    cur_col = (uint8_t)ncols;
                    AdjustCursorPosition(cur_col,cur_row);
                }

                /* JIS conversion to WORD value appropriate for text RAM */
                if (con_sjis.b2 != 0) con_sjis.b1 -= 0x20;

                INT10_WriteChar((con_sjis.b2 << 8) + con_sjis.b1,xAttr,0,1,true);

                cur_col += cw;
                AdjustCursorPosition(cur_col,cur_row);
                Real_INT10_SetCursorPos(cur_row,cur_col,page);	
            }
        }
        else {
            uint16_t oldax,oldbx;
            oldax=reg_ax;
            oldbx=reg_bx;

            reg_ah=0xE;
            reg_al=xChar;
            reg_bl=xAttr;

            CALLBACK_RunRealInt(0x10);

            reg_ax=oldax;
            reg_bx=oldbx;
        }
	}


	static void Real_WriteChar(uint8_t cur_col,uint8_t cur_row,
					uint8_t page,uint8_t chr,uint8_t attr,uint8_t useattr) {
		//Cursor position
		Real_INT10_SetCursorPos(cur_row,cur_col,page);

		//Write the character
		uint16_t		oldax,oldbx,oldcx;
		oldax=reg_ax;
		oldbx=reg_bx;
		oldcx=reg_cx;

		reg_al=chr;
		reg_bl=attr;
		reg_bh=page;
		reg_cx=1;
		if(useattr)
				reg_ah=0x9;
		else	reg_ah=0x0A;

        /* FIXME: PC-98 emulation should eventually use CONIO emulation that
         *        better emulates the actual platform. The purpose of this
         *        hack is to allow our code to call into INT 10h without
         *        setting up an INT 10h vector */
        if (IS_PC98_ARCH)
            INT10_Handler();
        else
            CALLBACK_RunRealInt(0x10);

		reg_ax=oldax;
		reg_bx=oldbx;
		reg_cx=oldcx;
	}//static void Real_WriteChar(cur_col,cur_row,page,chr,attr,useattr)

    static void LineFeedRev(void) { // ESC M
		BIOS_NCOLS;BIOS_NROWS;
		auto defattr = DefaultANSIAttr();
		uint8_t page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
		uint8_t cur_row=CURSOR_POS_ROW(page);
		uint8_t cur_col=CURSOR_POS_COL(page);

		if(cur_row==0) 
		{
            INT10_ScrollWindow(0,0,(uint8_t)(nrows-1),(uint8_t)(ncols-1),1,defattr,0);
        }
        else {
            cur_row--;
        }

		Real_INT10_SetCursorPos(cur_row,cur_col,page);	
    }

    static void LineFeed(void) { // ESC D
		BIOS_NCOLS;BIOS_NROWS;
		auto defattr = DefaultANSIAttr();
		uint8_t page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
		uint8_t cur_row=CURSOR_POS_ROW(page);
		uint8_t cur_col=CURSOR_POS_COL(page);

        if (cur_row < nrows) cur_row++;

		if(cur_row==nrows) 
		{
            INT10_ScrollWindow(0,0,(uint8_t)(nrows-1),(uint8_t)(ncols-1),-1,defattr,0);
            cur_row--;
		}

		Real_INT10_SetCursorPos(cur_row,cur_col,page);	
    }
	
	static void AdjustCursorPosition(uint8_t& cur_col,uint8_t& cur_row) {
		BIOS_NCOLS;BIOS_NROWS;
		auto defattr = DefaultANSIAttr();
		//Need a new line?
		if(cur_col==ncols) 
		{
			cur_col=0;
			cur_row++;

            if (!IS_PC98_ARCH)
                Real_INT10_TeletypeOutput('\r',defattr);
        }
		
		//Reached the bottom?
		if(cur_row==nrows) 
		{
            if (IS_PC98_ARCH)
		        INT10_ScrollWindow(0,0,(uint8_t)(nrows-1),(uint8_t)(ncols-1),-1,defattr,0);
            else
                Real_INT10_TeletypeOutput('\n',defattr);	//Scroll up

            cur_row--;
		}
	}


	void Real_INT10_TeletypeOutputAttr(uint8_t chr,uint8_t attr,bool useattr) {
		//TODO Check if this page thing is correct
		uint8_t page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
//		BIOS_NCOLS;BIOS_NROWS;
		uint8_t cur_row=CURSOR_POS_ROW(page);
		uint8_t cur_col=CURSOR_POS_COL(page);
		switch (chr) 
		{
		case 7: {
			// set timer (this should not be needed as the timer already is programmed 
			// with those values, but the speaker stays silent without it)
			IO_Write(0x43,0xb6);
			IO_Write(0x42,1320&0xff);
			IO_Write(0x42,1320>>8);
			// enable speaker
			IO_Write(0x61,IO_Read(0x61)|0x3);
			for(Bitu i=0; i < 333; i++) CALLBACK_Idle();
			IO_Write(0x61,IO_Read(0x61)&~0x3);
			break;
		}
		case 8:
			if(cur_col>0)
				cur_col--;
			break;
		case '\r':
			cur_col=0;
			break;
		case '\n':
			cur_col=0;
			cur_row++;
			break;
		case '\t':
			do {
				Real_INT10_TeletypeOutputAttr(' ',attr,useattr);
				cur_row=CURSOR_POS_ROW(page);
				cur_col=CURSOR_POS_COL(page);
			} while(cur_col%8);
			break;
		default:
			//* Draw the actual Character
            if (IS_PC98_ARCH) {
                if (con_sjis.take(chr)) {
                    BIOS_NCOLS;
                    unsigned char cw = con_sjis.doublewide ? 2 : 1;

                    /* FIXME: I'm not sure what NEC's ANSI driver does if a doublewide character is printed at column 79 */
                    if ((cur_col+cw) > ncols) {
                        cur_col = (uint8_t)ncols;
                        AdjustCursorPosition(cur_col,cur_row);
                    }

                    /* JIS conversion to WORD value appropriate for text RAM */
                    if (con_sjis.b2 != 0) con_sjis.b1 -= 0x20;

                    INT10_WriteChar((con_sjis.b2 << 8) + con_sjis.b1,attr,0,1,true);

                    cur_col += cw;
                }
            }
            else {
                Real_WriteChar(cur_col,cur_row,page,chr,attr,useattr);
                cur_col++;
            }
		}
		
		AdjustCursorPosition(cur_col,cur_row);
		Real_INT10_SetCursorPos(cur_row,cur_col,page);	
	}//void Real_INT10_TeletypeOutputAttr(uint8_t chr,uint8_t attr,bool useattr) 
public:
    // INT DC interface: CL=0x10 AH=0x03
    void INTDC_CL10h_AH03h(uint16_t raw) {
        /* NTS: This emulates translation behavior seen in INT DCh interface:
         *
         *      DX = raw
         *      DX += 0x2020
         *      XCHG DL, DH
         *      CX = DX
         *      CALL "ESC = HANDLING CODE"
         *
         * Technically this means there is a bug where if DL(X) is 0xE0 or larger it will carry into DH(Y) */
        raw += 0x2020;

        ansi.data[0] = (raw >> 8); // Y
        ansi.data[1] = raw & 0xFF; // X
        ESC_EQU_cursor_pos();
    }

    void INTDC_CL10h_AH04h(void) {
        ESC_D();
    }

    void INTDC_CL10h_AH05h(void) {
        ESC_M();
    }

    void INTDC_CL10h_AH06h(uint16_t count) {
        ansi.data[0] = (uint8_t)count; /* truncation is deliberate, just like the actual ANSI driver */
        ESC_BRACKET_A();
    }

    void INTDC_CL10h_AH07h(uint16_t count) {
        ansi.data[0] = (uint8_t)count; /* truncation is deliberate, just like the actual ANSI driver */
        ESC_BRACKET_B();
    }

    void INTDC_CL10h_AH08h(uint16_t count) {
        ansi.data[0] = (uint8_t)count; /* truncation is deliberate, just like the actual ANSI driver */
        ESC_BRACKET_C();
    }

    void INTDC_CL10h_AH09h(uint16_t count) {
        ansi.data[0] = (uint8_t)count; /* truncation is deliberate, just like the actual ANSI driver */
        ESC_BRACKET_D();
    }

};

// NEC-PC98 keyboard input notes
//
// on a system with KKCFUNC.SYS, NECAIK1.SYS, NECAIK2.SYS, NECAI.SYS loaded
//
// Key      Normal      Shift       CTRL
// -------------------------------------
// ESC      0x1B        0x1B        0x1B
// TAB      0x09        0x09        0x09
// F1       0x1B 0x53   <shortcut>  --
// F2       0x1B 0x54   <shortcut>  --
// F3       0x1B 0x55   <shortcut>  --
// F4       0x1B 0x56   <shortcut>  Toggles 'g'
// F5       0x1B 0x57   <shortcut>  --
// F6       0x1B 0x45   <shortcut>  Toggle 20/25-line text mode
// F7       0x1B 0x4A   <shortcut>  Toggle function row (C1/CU/etc, shortcuts, or off)
// F8       0x1B 0x50   <shortcut>  Clear screen, home cursor
// F9       0x1B 0x51   <shortcut>  --
// F10      0x1B 0x5A   <shortcut>  --
// INS      0x1B 0x50   0x1B 0x50   0x1B 0x50
// DEL      0x1B 0x44   0x1B 0x44   0x1B 0x44
// ROLL UP  --          --          --
// POLL DOWN--          --          --
// COPY     --          --          --
// HOME/CLR 0x1A        0x1E        --
// HELP     --          --          --
// UP ARROW 0x0B        0x0B        0x0B
// LF ARROW 0x08        0x08        0x08
// RT ARROW 0x0C        0x0C        0x0C
// DN ARROW 0x0A        0x0A        0x0A
// VF1      --          --          --
// VF2      --          --          --
// VF3      --          --          --
// VF4      --          --          --
// VF5      --          --          --

// TODO for PC-98 mode:
//
// According to:
//
// http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20NEC%20PC%2d98/Collections/PC%2d9801%20Bible%20%e6%9d%b1%e4%ba%ac%e7%90%86%e7%a7%91%e5%a4%a7%e5%ad%a6EIC%20%281994%29%2epdf
//
// Section 4-8.
//
// The PDF documents ANSI codes defined on PC-98, which may or may not be a complete listing.

bool device_CON::Read(uint8_t * data,uint16_t * size) {
	uint16_t oldax=reg_ax;
	uint16_t count=0;
	auto defattr=DefaultANSIAttr();
	INT10_SetCurMode();
	if ((readcache) && (*size)) {
		data[count++]=readcache;
		if(dos.echo) Real_INT10_TeletypeOutput(readcache,defattr);
		readcache=0;
	}
	while (*size>count) {
        if (dev_con_pos < dev_con_max) {
            data[count++] = (uint8_t)dev_con_readbuf[dev_con_pos++];
            continue;
        }

		reg_ah=(IS_EGAVGA_ARCH)?0x10:0x0;

        /* FIXME: PC-98 emulation should eventually use CONIO emulation that
         *        better emulates the actual platform. The purpose of this
         *        hack is to allow our code to call into INT 16h without
         *        setting up an INT 16h vector */
        if (IS_PC98_ARCH)
            INT16_Handler_Wrap();
        else
            CALLBACK_RunRealInt(0x16);

        /* hack for DOSKEY emulation */
        last_int16_code = reg_ax;

		switch(reg_al) {
		case 13:
			data[count++]=0x0D;
			if (*size>count) data[count++]=0x0A;    // it's only expanded if there is room for it. (NO cache)
			*size=count;
			reg_ax=oldax;
			if(dos.echo) { 
				Real_INT10_TeletypeOutput(13,defattr); //maybe don't do this ( no need for it actually ) (but it's compatible)
				Real_INT10_TeletypeOutput(10,defattr);
			}
			return true;
			break;
		case 8:
			if(*size==1) data[count++]=reg_al;  //one char at the time so give back that BS
			else if(count) {                    //Remove data if it exists (extended keys don't go right)
				data[count--]=0;
				Real_INT10_TeletypeOutput(8,defattr);
				Real_INT10_TeletypeOutput(' ',defattr);
			} else {
				continue;                       //no data read yet so restart whileloop.
			}
			break;
		case 0xe0: /* Extended keys in the  int 16 0x10 case */
			if(!reg_ah) { /*extended key if reg_ah isn't 0 */
				data[count++] = reg_al;
			} else {
				data[count++] = 0;
				if (*size>count) data[count++] = reg_ah;
				else readcache = reg_ah;
			}
			break;
		case 0: /* Extended keys in the int 16 0x0 case */
            if (reg_ax == 0) { /* CTRL+BREAK hackery (inserted as 0x0000) */
    			data[count++]=0x03; // CTRL+C
                if (*size > 1 || !inshell) {
                    dos.errorcode=77;
                    *size=count;
                    reg_ax=oldax;
                    return false;
                }
            }
            else if (IS_PC98_ARCH) {
                /* PC-98 does NOT return scan code, but instead returns nothing or
                 * control/escape code */
                CommonPC98ExtScanConversionToReadBuf(reg_ah);
            }
            else {
                /* IBM PC/XT/AT signals extended code by entering AL, AH.
                 * Arrow keys for example become 0x00 0x48, 0x00 0x50, etc. */
    			data[count++]=reg_al;
	    		if (*size>count) data[count++]=reg_ah;
		    	else readcache=reg_ah;
            }
			break;
		default:
			data[count++]=reg_al;
			if ((*size > 1 || !inshell) && reg_al == 3) {
				dos.errorcode=77;
				*size=count;
				reg_ax=oldax;
				return false;
			}
			break;
		}
		if(dos.echo) { //what to do if *size==1 and character is BS ?????
			// TODO: If CTRL+C checking is applicable do not echo (reg_al == 3)
			Real_INT10_TeletypeOutput(reg_al,defattr);
		}
	}
	dos.errorcode=0;
	*size=count;
	reg_ax=oldax;
	return true;
}

bool log_dev_con = false;
std::string log_dev_con_str;

bool device_CON::Write(const uint8_t * data,uint16_t * size) {
    uint16_t count=0;
    Bitu i;
    uint8_t col,row,page;

    INT10_SetCurMode();

    if (IS_PC98_ARCH) {
        ansi.enabled = true; // ANSI is enabled at all times
        ansi.attr = mem_readb(0x71D); // 60:11D
    }

    while (*size>count) {
        if (log_dev_con) {
            if (log_dev_con_str.size() >= 255 || data[count] == '\n' || data[count] == 27) {
                LOG_MSG("DOS CON: %s",log_dev_con_str.c_str());
                log_dev_con_str.clear();
            }

            if (data[count] != '\n' && data[count] != '\r')
                log_dev_con_str += (char)data[count];
        }

        if (!ansi.esc){
            if(data[count]=='\033' && ansi.installed) {
                /*clear the datastructure */
                ClearAnsi();
                /* start the sequence */
                ansi.esc=true;
                count++;
                continue;
            } else if(data[count] == '\t' && !dos.direct_output) {
                /* expand tab if not direct output */
                page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
                do {
                    Output(' ');
                    col=CURSOR_POS_COL(page);
                } while(col%8);
                count++;
                continue;
            } else if (data[count] == 0x1A && IS_PC98_ARCH) {
                page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);

                /* it also redraws the function key row */
                update_pc98_function_row(pc98_function_row_mode,true);

                INT10_ScrollWindow(0,0,255,255,0,ansi.attr,page);
                Real_INT10_SetCursorPos(0,0,page);
            } else if (data[count] == 0x1E && IS_PC98_ARCH) {
                page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);

                Real_INT10_SetCursorPos(0,0,page);
            } else { 
                Output(data[count]);
                count++;
                continue;
            }
        }

        if(!ansi.sci){

            switch(data[count]){
                case '[': 
                    ansi.sci=true;
                    break;
                case '*':/* PC-98: clear screen (same code path as CTRL+Z) */
                    if (IS_PC98_ARCH) {
                        page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);

                        /* it also redraws the function key row */
                        update_pc98_function_row(pc98_function_row_mode,true);

                        INT10_ScrollWindow(0,0,255,255,0,ansi.attr,page);
                        Real_INT10_SetCursorPos(0,0,page);
                        ClearAnsi();
                    }
                    else {
                        LOG(LOG_IOCTL,LOG_NORMAL)("ANSI: unknown char %c after a esc",data[count]); /*prob () */
                        ClearAnsi();
                    }
                    break;
                case 'D':/* cursor DOWN (with scrolling) */
                    ESC_D();
                    break;
                case 'E':/* cursor DOWN, carriage return (with scrolling) */
                    ESC_E();
                    break;
                case 'M':/* cursor UP (with scrolling) */ 
                    ESC_M();
                    break;
                case '=':/* cursor position */
                    ansi.equcurp=true;
                    ansi.sci=true;
                    break;
                case '7': /* save cursor pos + attr TODO */
                case '8': /* restore this TODO */
                default:
                    LOG(LOG_IOCTL,LOG_NORMAL)("ANSI: unknown char %c after a esc",data[count]); /*prob () */
                    ClearAnsi();
                    break;
            }
            count++;
            continue;
        }
        /*ansi.esc and ansi.sci are true */
        if (!dos.internal_output) ansi.enabled=true;
        page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
        if (ansi.equcurp) { /* proprietary ESC = Y X command */
            ansi.data[ansi.numberofarg++] = data[count];
            if (ansi.numberofarg >= 2) {
                ESC_EQU_cursor_pos(); /* clears ANSI state */
            }
        }
        else if (isdigit(data[count])) {
            assert(ansi.numberofarg < NUMBER_ANSI_DATA);
            ansi.data[ansi.numberofarg]=10*ansi.data[ansi.numberofarg]+(data[count]-'0');
        }
        else if (data[count] == ';') {
            if ((ansi.numberofarg+1) < NUMBER_ANSI_DATA)
                ansi.numberofarg++;
        }
        else if (ansi.pc98rab) {
            assert(IS_PC98_ARCH);

            switch(data[count]){
                case 'h': /* SET   MODE (if code =7 enable linewrap) */
                case 'l': /* RESET MODE */
                    switch (ansi.data[0]) {
                        case 1: // show/hide function key row
                            update_pc98_function_row(data[count] == 'l');
                            ansi.nrows = real_readb(0x60,0x112)+1;
                            break;
                        case 3: // clear screen (doesn't matter if l or h)
                            INT10_ScrollWindow(0,0,255,255,0,ansi.attr,page);
                            Real_INT10_SetCursorPos(0,0,page);
                            break;
                        case 5: // show/hide cursor
                            void PC98_show_cursor(bool show);
                            PC98_show_cursor(data[count] == 'l');
                            mem_writeb(0x71B,data[count] == 'l' ? 0x01 : 0x00); /* 60:11B cursor display state */
                            break;
                        default:
                            LOG(LOG_IOCTL,LOG_NORMAL)("ANSI: unhandled esc [ > %d %c",ansi.data[0],data[count]);
                            break;
                    };

                    ClearAnsi();
                    break;
                default:
                    LOG(LOG_IOCTL,LOG_NORMAL)("ANSI: unhandled char %c in esc [ >",data[count]);
                    ClearAnsi();
                    break;
            }
        }
        else {
            switch(data[count]){
                case 'm':               /* SGR */
                    // NEC's ANSI driver always resets at the beginning
                    if(IS_PC98_ARCH) {
                        ansi.attr = DefaultANSIAttr();
                    }
                    for(i=0;i<=ansi.numberofarg;i++){ 
                        const uint8_t COLORFLAGS[][8] = {
                        //  Black   Red Green Yellow Blue  Pink  Cyan  White
                            { 0x0,  0x4,  0x2,  0x6,  0x1,  0x5,  0x3,  0x7 }, /*   IBM */
                            { 0x0, 0x40, 0x80, 0xC0, 0x20, 0x60, 0xA0, 0xE0 }, /* PC-98 */
                        };
                        const auto &flagset = COLORFLAGS[IS_PC98_ARCH];

                        if(IS_PC98_ARCH) {
                            // Convert alternate color codes to regular ones
                            if(ansi.data[i] >= 17 && ansi.data[i] <= 23) {
                                const uint8_t convtbl[] = {
                                    31, 34, 35, 32, 33, 36, 37
                                };
                                ansi.data[i] = convtbl[ansi.data[i] - 17];
                            }
                        }

                        switch(ansi.data[i]){
                            case 0: /* normal */
                                //Real ansi does this as well. (should do current defaults)
                                ansi.attr = DefaultANSIAttr();
                                break;
                            case 1: /* bold mode on*/
                                // FIXME: According to http://www.ninton.co.jp/?p=11, this
                                // should set some sort of "highlight" flag in monochrome
                                // mode, but I have no idea how to even enter that mode.
                                ansi.attr |= IS_PC98_ARCH ? 0 : 0x08;
                                break;
                            case 2: /* PC-98 "Bit 4" */
                                ansi.attr |= IS_PC98_ARCH ? 0x10 : 0;
                                break;
                            case 4: /* underline */
                                if(IS_PC98_ARCH) {
                                    ansi.attr |= 0x08;
                                } else {
                                    LOG(LOG_IOCTL, LOG_NORMAL)("ANSI:no support for underline yet");
                                }
                                break;
                            case 5: /* blinking */
                                ansi.attr |= IS_PC98_ARCH ? 0x02 : 0x80;
                                break;
                            case 7: /* reverse */
                                //Just like real ansi. (should do use current colors reversed)
                                if(IS_PC98_ARCH) {
                                    ansi.attr |= 0x04;
                                } else {
                                    ansi.attr = 0x70;
                                }
                                break;
                            case 8: /* PC-98 secret */
                            case 16:
                                ansi.attr &= IS_PC98_ARCH ? 0xFE : 0xFF;
                                break;
                            case 30: /* fg color black */
                            case 31: /* fg color red */
                            case 32: /* fg color green */
                            case 33: /* fg color yellow */
                            case 34: /* fg color blue */
                            case 35: /* fg color magenta */
                            case 36: /* fg color cyan */
                            case 37: /* fg color white */
                                ansi.attr &= ~(flagset[7]);
                                ansi.attr |= (flagset[ansi.data[i] - 30]);
                                break;
                            case 40:
                            case 41:
                            case 42:
                            case 43:
                            case 44:
                            case 45:
                            case 46:
                            case 47: {
                                uint8_t shift = IS_PC98_ARCH ? 0 : 4;
                                ansi.attr &= ~(flagset[7] << shift);
                                ansi.attr |= (flagset[ansi.data[i] - 40] << shift);
                                ansi.attr |= IS_PC98_ARCH ? 0x04 : 0;
                                break;
                            }
                            default:
                                break;
                        }
                    }
                    if (IS_PC98_ARCH) mem_writeb(0x71D,ansi.attr); // 60:11D
                    ClearAnsi();
                    break;
                case 'f':
                case 'H':/* Cursor Pos*/
                    if(!ansi.warned) { //Inform the debugger that ansi is used.
                        ansi.warned = true;
                        LOG(LOG_IOCTL,LOG_WARN)("ANSI SEQUENCES USED");
                    }
                    if (!IS_PC98_ARCH) {
                        ansi.ncols = real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
                        ansi.nrows = real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS) + 1;
                    }
                    /* Turn them into positions that are on the screen */
                    if(ansi.data[0] == 0) ansi.data[0] = 1;
                    if(ansi.data[1] == 0) ansi.data[1] = 1;
                    if(ansi.data[0] > ansi.nrows) ansi.data[0] = (uint8_t)ansi.nrows;
                    if(ansi.data[1] > ansi.ncols) ansi.data[1] = (uint8_t)ansi.ncols;
                    Real_INT10_SetCursorPos(--(ansi.data[0]),--(ansi.data[1]),page); /*ansi=1 based, int10 is 0 based */
                    ClearAnsi();
                    break;
                    /* cursor up down and forward and backward only change the row or the col not both */
                case 'A': /* cursor up*/
                    ESC_BRACKET_A();
                    break;
                case 'B': /*cursor Down */
                    ESC_BRACKET_B();
                    break;
                case 'C': /*cursor forward */
                    ESC_BRACKET_C();
                    break;
                case 'D': /*Cursor Backward  */
                    ESC_BRACKET_D();
                    break;
                case 'J': /*erase screen and move cursor home*/
                    if(ansi.data[0]==0) ansi.data[0]=2;
                    if(ansi.data[0]!=2) {/* every version behaves like type 2 */
                        LOG(LOG_IOCTL,LOG_NORMAL)("ANSI: esc[%dJ called : not supported handling as 2",ansi.data[0]);
                    }
                    INT10_ScrollWindow(0,0,255,255,0,ansi.attr,page);
                    ClearAnsi();
                    Real_INT10_SetCursorPos(0,0,page);
                    break;
                case 'h': /* SET   MODE (if code =7 enable linewrap) */
                case 'I': /* RESET MODE */
                    LOG(LOG_IOCTL,LOG_NORMAL)("ANSI: set/reset mode called(not supported)");
                    ClearAnsi();
                    break;
                case 'u': /* Restore Cursor Pos */
                    Real_INT10_SetCursorPos(ansi.saverow,ansi.savecol,page);
                    ClearAnsi();
                    break;
                case 's': /* SAVE CURSOR POS */
                    ansi.savecol=CURSOR_POS_COL(page);
                    ansi.saverow=CURSOR_POS_ROW(page);
                    ClearAnsi();
                    break;
                case 'K': /* erase till end of line (don't touch cursor) */
                    col = CURSOR_POS_COL(page);
                    row = CURSOR_POS_ROW(page);
                    if (!IS_PC98_ARCH) {
                        ansi.ncols = real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
                    }
					INT10_WriteChar(' ',ansi.attr,page,ansi.ncols-col,true); //Real_WriteChar(ansi.ncols-col,row,page,' ',ansi.attr,true);

                    //for(i = col;i<(Bitu) ansi.ncols; i++) INT10_TeletypeOutputAttr(' ',ansi.attr,true);
                    Real_INT10_SetCursorPos(row,col,page);
                    ClearAnsi();
                    break;
                case 'M': /* delete line (NANSI) */
                    row = CURSOR_POS_ROW(page);
                    if (!IS_PC98_ARCH) {
                        ansi.ncols = real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
                        ansi.nrows = real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS) + 1;
                    }
					INT10_ScrollWindow(row,0,ansi.nrows-1,ansi.ncols-1,ansi.data[0]? -ansi.data[0] : -1,ansi.attr,0xFF);
                    ClearAnsi();
                    break;
                case '>':/* proprietary NEC PC-98 MS-DOS codes (??) */
                    if (IS_PC98_ARCH) {
                        ansi.pc98rab = true;
                    }
                    else {
                        LOG(LOG_IOCTL,LOG_NORMAL)("ANSI: ESC [ > not supported outside PC-98 mode");
                        ClearAnsi();
                    }
                    break;
                case 'l':/* (if code =7) disable linewrap */
                case 'p':/* reassign keys (needs strings) */
                case 'i':/* printer stuff */
                default:
                    LOG(LOG_IOCTL,LOG_NORMAL)("ANSI: unhandled char %c in esc[",data[count]);
                    ClearAnsi();
                    break;
            }
        }
        count++;
    }
    *size=count;
    return true;
}

bool device_CON::Seek(uint32_t * pos,uint32_t type) {
    (void)type; // UNUSED
	// seek is valid
	*pos = 0;
	return true;
}

bool device_CON::Close() {
	return true;
}

extern bool dos_con_use_int16_to_detect_input;

uint16_t device_CON::GetInformation(void) {
	if (dos_con_use_int16_to_detect_input || IS_PC98_ARCH) {
		uint16_t ret = 0x80D3; /* No Key Available */

		/* DOSBox-X behavior: Use INT 16h AH=0x11 Query keyboard status/preview key.
		 * The reason we do this is some DOS programs actually rely on hooking INT 16h
		 * to manipulate, hide, or transform what the DOS CON driver sees as well as
		 * itself. Perhaps the most disgusting example of this behavior would be the
		 * SCANDISK.EXE utility in Microsoft MS-DOS 6.22, which apparently relies on
		 * hooking INT 16h in this way to catch the Escape, CTRL+C, and some other
		 * scan codes in order to "eat" the scan codes before they get back to DOS.
		 * The reason they can get away with it apparently and still respond properly
		 * to those keys, is because the MS-DOS 6.22 CON driver always calls INT 16h
		 * AH=0x11 first before calling INT 16h AH=0x10 to fetch the scan code.
		 *
		 * Without this fix, SCANDISK.EXE does not respond properly to Escape and
		 * a few other keys. Pressing Escape will do nothing until you hit any other
		 * key, at which point it suddenly acts upon the Escape key.
		 *
		 * Since Scandisk is using INT 21h AH=0x0B to query STDIN during this time,
		 * this implementation is a good "halfway" compromise in that this call
		 * will trigger the INT 16h AH=0x11 hook it relies on. */
		if (readcache || dev_con_pos < dev_con_max) return 0x8093; /* key available */

		if (DOS_BreakConioFlag) return 0x8093; /* key available */

		uint16_t saved_ax = reg_ax;

		reg_ah = (IS_EGAVGA_ARCH)?0x11:0x1; // check for keystroke

        /* FIXME: PC-98 emulation should eventually use CONIO emulation that
         *        better emulates the actual platform. The purpose of this
         *        hack is to allow our code to call into INT 16h without
         *        setting up an INT 16h vector */
        if (IS_PC98_ARCH)
            INT16_Handler_Wrap();
        else
            CALLBACK_RunRealInt(0x16);

        if (!GETFLAG(ZF)) { /* key is present, waiting to be returned on AH=0x10 or AH=0x00 */
            if (IS_PC98_ARCH && reg_al == 0) {
                /* some scan codes are ignored by CON, and wouldn't read anything.
                 * while we're at it, take the scan code and convert it into ANSI here
                 * so that Read() returns it immediately instead of doing this conversion itself.
                 * This way we never block when we SAID a key was available that gets ignored. */
                if (CommonPC98ExtScanConversionToReadBuf(reg_ah))
                    ret = 0x8093; /* Key Available */
                else
                    ret = 0x80D3; /* No Key Available */

                /* need to consume the key. if it generated anything it will be returned to Read()
                 * through dev_con_readbuf[] */
                reg_ah=0x0;

                /* FIXME: PC-98 emulation should eventually use CONIO emulation that
                 *        better emulates the actual platform. The purpose of this
                 *        hack is to allow our code to call into INT 16h without
                 *        setting up an INT 16h vector */
                INT16_Handler_Wrap();
            }
            else {
                ret = 0x8093; /* Key Available */
            }
        }

		reg_ax = saved_ax;
		return ret;
	}
	else {
		/* DOSBox mainline behavior: alternate "fast" way through direct manipulation of keyboard scan buffer */
		uint16_t head=mem_readw(BIOS_KEYBOARD_BUFFER_HEAD);
		uint16_t tail=mem_readw(BIOS_KEYBOARD_BUFFER_TAIL);

		if ((head==tail) && !readcache) return 0x80D3;	/* No Key Available */
		if (readcache || real_readw(0x40,head)) return 0x8093;		/* Key Available */

		/* remove the zero from keyboard buffer */
		uint16_t start=mem_readw(BIOS_KEYBOARD_BUFFER_START);
		uint16_t end	=mem_readw(BIOS_KEYBOARD_BUFFER_END);
		head+=2;
		if (head>=end) head=start;
		mem_writew(BIOS_KEYBOARD_BUFFER_HEAD,head);
	}

	return 0x80D3; /* No Key Available */
}

device_CON::device_CON() {
	Section_prop *section=static_cast<Section_prop *>(control->GetSection("dos"));

	SetName("CON");
	readcache=0;

    if (IS_PC98_ARCH) {
        /* On real MS-DOS for PC-98, ANSI.SYS is effectively part of the DOS kernel, and cannot be turned off. */
        ansi.installed=true;
    }
    else {
        /* Otherwise (including IBM systems), ANSI.SYS is not installed by default but can be added to CONFIG.SYS.
         * For compatibility with DOSBox SVN and other forks ANSI.SYS is installed by default. */
        ansi.installed=section->Get_bool("ansi.sys");
    }

	ansi.enabled=false;
	ansi.attr=DefaultANSIAttr();
    if (IS_PC98_ARCH) {
        // NTS: On real hardware, the BIOS does NOT manage the console at all.
        //      TTY handling is entirely handled by MS-DOS.
        ansi.ncols=80;
        ansi.nrows=25 - 1;
        // the DOS kernel will call on this function to disable, and SDLmain
        // will call on to enable
    }
	ansi.saverow=0;
	ansi.savecol=0;
	ansi.warned=false;
	ClearAnsi();
}

void device_CON::ClearAnsi(void){
	for(uint8_t i=0; i<NUMBER_ANSI_DATA;i++) ansi.data[i]=0;
    ansi.pc98rab=false;
    ansi.equcurp=false;
	ansi.esc=false;
	ansi.sci=false;
	ansi.numberofarg=0;
}

void device_CON::Output(uint8_t chr) {
	if (!ANSI_SYS_installed() && !IS_PC98_ARCH) {
		uint16_t oldax=reg_ax;
		reg_ax=chr;
		CALLBACK_RunRealInt(0x29);
		reg_ax=oldax;
	} else if (dos.internal_output || ansi.enabled) {
		if (CurMode->type==M_TEXT) {
			uint8_t page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
			uint8_t col=CURSOR_POS_COL(page);
			uint8_t row=CURSOR_POS_ROW(page);
			BIOS_NCOLS;BIOS_NROWS;
			if (nrows==row+1 && (chr=='\n' || (ncols==col+1 && chr!='\r' && chr!=8 && chr!=7))) {
				INT10_ScrollWindow(0,0,(uint8_t)(nrows-1),(uint8_t)(ncols-1),-1,ansi.attr,page);
				INT10_SetCursorPos(row-1,col,page);
			}
		}
		Real_INT10_TeletypeOutputAttr(chr,ansi.attr,true);
	} else Real_INT10_TeletypeOutput(chr,DefaultANSIAttr());
}

bool device_CON::ANSI_SYS_installed() {
    return ansi.installed;
}

