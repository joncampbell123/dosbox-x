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


#include "dos_inc.h"
#include "../ints/int10.h"
#include <string.h>
#include "inout.h"
#include "shiftjis.h"
#include "callback.h"

#define NUMBER_ANSI_DATA 10

extern bool DOS_BreakFlag;

Bitu INT10_Handler(void);
Bitu INT16_Handler_Wrap(void);

ShiftJISDecoder con_sjis;

Bit16u last_int16_code = 0;

static size_t dev_con_pos=0,dev_con_max=0;
static char dev_con_readbuf[64];

class device_CON : public DOS_Device {
public:
	device_CON();
	bool Read(Bit8u * data,Bit16u * size);
	bool Write(Bit8u * data,Bit16u * size);
	bool Seek(Bit32u * pos,Bit32u type);
	bool Close();
	void ClearAnsi(void);
	Bit16u GetInformation(void);
	bool ReadFromControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode) { (void)bufptr; (void)size; (void)retcode; return false; }
	bool WriteToControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode) { (void)bufptr; (void)size; (void)retcode; return false; }
private:
	Bit8u readcache;
	Bit8u lastwrite;
	struct ansi { /* should create a constructor, which would fill them with the appropriate values */
		bool esc;
		bool sci;
        bool pc98rab;       // PC-98 ESC [ > ...    (right angle bracket) I will rename this variable if MS-DOS ANSI.SYS also supports this sequence
		bool enabled;
		Bit8u attr;
		Bit8u data[NUMBER_ANSI_DATA];
		Bit8u numberofarg;
		Bit16u nrows;
		Bit16u ncols;
		Bit8s savecol;
		Bit8s saverow;
		bool warned;
	} ansi;

	static void Real_INT10_SetCursorPos(Bit8u row,Bit8u col,Bit8u page) {
		Bit16u		oldax,oldbx,olddx;

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
        switch (code) {
            case 0x39: // DEL
                dev_con_readbuf[0] = 0x1B; dev_con_readbuf[1] = 0x44; dev_con_pos=0; dev_con_max=2;
                return true;
            case 0x3A: // up arrow
                dev_con_readbuf[0] = 0x0B; dev_con_pos=0; dev_con_max=1;
                return true;
            case 0x3B: // left arrow
                dev_con_readbuf[0] = 0x08; dev_con_pos=0; dev_con_max=1;
                return true;
            case 0x3C: // right arrow
                dev_con_readbuf[0] = 0x0C; dev_con_pos=0; dev_con_max=1;
                return true;
            case 0x3D: // down arrow
                dev_con_readbuf[0] = 0x0A; dev_con_pos=0; dev_con_max=1;
                return true;
            case 0x62: // F1
                dev_con_readbuf[0] = 0x1B; dev_con_readbuf[1] = 0x53; dev_con_pos=0; dev_con_max=2;
                return true;
            case 0x63: // F2
                dev_con_readbuf[0] = 0x1B; dev_con_readbuf[1] = 0x54; dev_con_pos=0; dev_con_max=2;
                return true;
            case 0x64: // F3
                dev_con_readbuf[0] = 0x1B; dev_con_readbuf[1] = 0x55; dev_con_pos=0; dev_con_max=2;
                return true;
            case 0x65: // F4
                dev_con_readbuf[0] = 0x1B; dev_con_readbuf[1] = 0x56; dev_con_pos=0; dev_con_max=2;
                return true;
            case 0x66: // F5
                dev_con_readbuf[0] = 0x1B; dev_con_readbuf[1] = 0x57; dev_con_pos=0; dev_con_max=2;
                return true;
            case 0x67: // F6
                dev_con_readbuf[0] = 0x1B; dev_con_readbuf[1] = 0x45; dev_con_pos=0; dev_con_max=2;
                return true;
            case 0x68: // F7
                dev_con_readbuf[0] = 0x1B; dev_con_readbuf[1] = 0x4A; dev_con_pos=0; dev_con_max=2;
                return true;
            case 0x69: // F8
                dev_con_readbuf[0] = 0x1B; dev_con_readbuf[1] = 0x50; dev_con_pos=0; dev_con_max=2;
                return true;
            case 0x6A: // F9
                dev_con_readbuf[0] = 0x1B; dev_con_readbuf[1] = 0x51; dev_con_pos=0; dev_con_max=2;
                return true;
            case 0x6B: // F10
                dev_con_readbuf[0] = 0x1B; dev_con_readbuf[1] = 0x5A; dev_con_pos=0; dev_con_max=2;
                return true;
#if 0
                // INS      0x1B 0x50   0x1B 0x50   0x1B 0x50
                // ROLL UP  --          --          --
                // POLL DOWN--          --          --
                // COPY     --          --          --
                // HOME/CLR 0x1A        0x1E        --
                // HELP     --          --          --
#endif
        }

        return false;
    }

	static void Real_INT10_TeletypeOutput(Bit8u xChar,Bit8u xAttr) {
		Bit16u		oldax,oldbx;

        if (IS_PC98_ARCH) {
            if (con_sjis.take(xChar)) {
                BIOS_NCOLS;
                Bit8u page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
                Bit8u cur_row=CURSOR_POS_ROW(page);
                Bit8u cur_col=CURSOR_POS_COL(page);
                unsigned char cw = con_sjis.doublewide ? 2 : 1;

                /* FIXME: I'm not sure what NEC's ANSI driver does if a doublewide character is printed at column 79 */
                if ((cur_col+cw) > ncols) {
                    cur_col = ncols;
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


	static void Real_WriteChar(Bit8u cur_col,Bit8u cur_row,
					Bit8u page,Bit8u chr,Bit8u attr,Bit8u useattr) {
		//Cursor position
		Real_INT10_SetCursorPos(cur_row,cur_col,page);

		//Write the character
		Bit16u		oldax,oldbx,oldcx;
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

	
	static void AdjustCursorPosition(Bit8u& cur_col,Bit8u& cur_row) {
		BIOS_NCOLS;BIOS_NROWS;
		//Need a new line?
		if(cur_col==ncols) 
		{
			cur_col=0;
			cur_row++;

            if (!IS_PC98_ARCH)
                Real_INT10_TeletypeOutput('\r',0x7);
        }
		
		//Reached the bottom?
		if(cur_row==nrows) 
		{
            if (IS_PC98_ARCH)
		        INT10_ScrollWindow(0,0,(Bit8u)(nrows-1),(Bit8u)(ncols-1),-1,0x07,0);
            else
                Real_INT10_TeletypeOutput('\n',0x7);	//Scroll up

            cur_row--;
		}
	}


	void Real_INT10_TeletypeOutputAttr(Bit8u chr,Bit8u attr,bool useattr) {
		//TODO Check if this page thing is correct
		Bit8u page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
//		BIOS_NCOLS;BIOS_NROWS;
		Bit8u cur_row=CURSOR_POS_ROW(page);
		Bit8u cur_col=CURSOR_POS_COL(page);
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
                        cur_col = ncols;
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
	}//void Real_INT10_TeletypeOutputAttr(Bit8u chr,Bit8u attr,bool useattr) 
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

bool device_CON::Read(Bit8u * data,Bit16u * size) {
	Bit16u oldax=reg_ax;
	Bit16u count=0;
	INT10_SetCurMode();
	if ((readcache) && (*size)) {
		data[count++]=readcache;
		if(dos.echo) Real_INT10_TeletypeOutput(readcache,7);
		readcache=0;
	}
	while (*size>count) {
        if (dev_con_pos < dev_con_max) {
            data[count++] = dev_con_readbuf[dev_con_pos++];
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
				Real_INT10_TeletypeOutput(13,7); //maybe don't do this ( no need for it actually ) (but it's compatible)
				Real_INT10_TeletypeOutput(10,7);
			}
			return true;
			break;
		case 8:
			if(*size==1) data[count++]=reg_al;  //one char at the time so give back that BS
			else if(count) {                    //Remove data if it exists (extended keys don't go right)
				data[count--]=0;
				Real_INT10_TeletypeOutput(8,7);
				Real_INT10_TeletypeOutput(' ',7);
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
            if (IS_PC98_ARCH) {
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
			break;
		}
		if(dos.echo) { //what to do if *size==1 and character is BS ?????
			// TODO: If CTRL+C checking is applicable do not echo (reg_al == 3)
			Real_INT10_TeletypeOutput(reg_al,7);
		}
	}
	*size=count;
	reg_ax=oldax;
	return true;
}

bool log_dev_con = false;
std::string log_dev_con_str;

bool device_CON::Write(Bit8u * data,Bit16u * size) {
    Bit16u count=0;
    Bitu i;
    Bit8u col,row;
    Bit8u tempdata;
    INT10_SetCurMode();
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
            if(data[count]=='\033') {
                /*clear the datastructure */
                ClearAnsi();
                /* start the sequence */
                ansi.esc=true;
                count++;
                continue;
            } else { 
                /* Some sort of "hack" now that '\n' doesn't set col to 0 (int10_char.cpp old chessgame) */
                if((data[count] == '\n') && (lastwrite != '\r')) Real_INT10_TeletypeOutputAttr('\r',ansi.attr,ansi.enabled);
                /* use ansi attribute if ansi is enabled, otherwise use DOS default attribute*/
                Real_INT10_TeletypeOutputAttr(data[count],ansi.enabled?ansi.attr:7,true);
                lastwrite = data[count++];
                continue;
            }
        }

        if(!ansi.sci){

            switch(data[count]){
                case '[': 
                    ansi.sci=true;
                    break;
                case '7': /* save cursor pos + attr */
                case '8': /* restore this  (Wonder if this is actually used) */
                case 'D':/* scrolling DOWN*/
                case 'M':/* scrolling UP*/ 
                case '*':/* PC-98: clear screen */
                    if (IS_PC98_ARCH) {
                        Bit8u page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);

                        INT10_ScrollWindow(0,0,255,255,0,ansi.attr,page);
                        Real_INT10_SetCursorPos(0,0,page);
                        ClearAnsi();
                        break;
                    }
                    else {
                        /* fall through */
                    }
                default:
                    LOG(LOG_IOCTL,LOG_NORMAL)("ANSI: unknown char %c after a esc",data[count]); /*prob () */
                    ClearAnsi();
                    break;
            }
            count++;
            continue;
        }
        /*ansi.esc and ansi.sci are true */
        Bit8u page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
        if (isdigit(data[count])) {
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
                            void update_pc98_function_row(bool enable);
                            update_pc98_function_row(data[count] == 'l');
                            break;
                        case 3: // clear screen (doesn't matter if l or h)
                            INT10_ScrollWindow(0,0,255,255,0,ansi.attr,page);
                            Real_INT10_SetCursorPos(0,0,page);
                            break;
                        case 5: // show/hide cursor
                            void PC98_show_cursor(bool show);
                            PC98_show_cursor(data[count] == 'l');
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
                    for(i=0;i<=ansi.numberofarg;i++){ 
                        ansi.enabled=true;
                        switch(ansi.data[i]){
                            case 0: /* normal */
                                ansi.attr=0x07;//Real ansi does this as well. (should do current defaults)
                                ansi.enabled=false;
                                break;
                            case 1: /* bold mode on*/
                                ansi.attr|=0x08;
                                break;
                            case 4: /* underline */
                                LOG(LOG_IOCTL,LOG_NORMAL)("ANSI:no support for underline yet");
                                break;
                            case 5: /* blinking */
                                ansi.attr|=0x80;
                                break;
                            case 7: /* reverse */
                                ansi.attr=0x70;//Just like real ansi. (should do use current colors reversed)
                                break;
                            case 30: /* fg color black */
                                ansi.attr&=0xf8;
                                ansi.attr|=0x0;
                                break;
                            case 31:  /* fg color red */
                                ansi.attr&=0xf8;
                                ansi.attr|=0x4;
                                break;
                            case 32:  /* fg color green */
                                ansi.attr&=0xf8;
                                ansi.attr|=0x2;
                                break;
                            case 33: /* fg color yellow */
                                ansi.attr&=0xf8;
                                ansi.attr|=0x6;
                                break;
                            case 34: /* fg color blue */
                                ansi.attr&=0xf8;
                                ansi.attr|=0x1;
                                break;
                            case 35: /* fg color magenta */
                                ansi.attr&=0xf8;
                                ansi.attr|=0x5;
                                break;
                            case 36: /* fg color cyan */
                                ansi.attr&=0xf8;
                                ansi.attr|=0x3;
                                break;
                            case 37: /* fg color white */
                                ansi.attr&=0xf8;
                                ansi.attr|=0x7;
                                break;
                            case 40:
                                ansi.attr&=0x8f;
                                ansi.attr|=0x0;
                                break;
                            case 41:
                                ansi.attr&=0x8f;
                                ansi.attr|=0x40;
                                break;
                            case 42:
                                ansi.attr&=0x8f;
                                ansi.attr|=0x20;
                                break;
                            case 43:
                                ansi.attr&=0x8f;
                                ansi.attr|=0x60;
                                break;
                            case 44:
                                ansi.attr&=0x8f;
                                ansi.attr|=0x10;
                                break;
                            case 45:
                                ansi.attr&=0x8f;
                                ansi.attr|=0x50;
                                break;
                            case 46:
                                ansi.attr&=0x8f;
                                ansi.attr|=0x30;
                                break;	
                            case 47:
                                ansi.attr&=0x8f;
                                ansi.attr|=0x70;
                                break;
                            default:
                                break;
                        }
                    }
                    ClearAnsi();
                    break;
                case 'f':
                case 'H':/* Cursor Pos*/
                    if(!ansi.warned) { //Inform the debugger that ansi is used.
                        ansi.warned = true;
                        LOG(LOG_IOCTL,LOG_WARN)("ANSI SEQUENCES USED");
                    }
                    /* Turn them into positions that are on the screen */
                    if(ansi.data[0] == 0) ansi.data[0] = 1;
                    if(ansi.data[1] == 0) ansi.data[1] = 1;
                    if(ansi.data[0] > ansi.nrows) ansi.data[0] = (Bit8u)ansi.nrows;
                    if(ansi.data[1] > ansi.ncols) ansi.data[1] = (Bit8u)ansi.ncols;
                    Real_INT10_SetCursorPos(--(ansi.data[0]),--(ansi.data[1]),page); /*ansi=1 based, int10 is 0 based */
                    ClearAnsi();
                    break;
                    /* cursor up down and forward and backward only change the row or the col not both */
                case 'A': /* cursor up*/
                    col=CURSOR_POS_COL(page) ;
                    row=CURSOR_POS_ROW(page) ;
                    tempdata = (ansi.data[0]? ansi.data[0] : 1);
                    if(tempdata > row) { row=0; } 
                    else { row-=tempdata;}
                    Real_INT10_SetCursorPos(row,col,page);
                    ClearAnsi();
                    break;
                case 'B': /*cursor Down */
                    col=CURSOR_POS_COL(page) ;
                    row=CURSOR_POS_ROW(page) ;
                    tempdata = (ansi.data[0]? ansi.data[0] : 1);
                    if(tempdata + static_cast<Bitu>(row) >= ansi.nrows)
                    { row = ansi.nrows - 1;}
                    else	{ row += tempdata; }
                    Real_INT10_SetCursorPos(row,col,page);
                    ClearAnsi();
                    break;
                case 'C': /*cursor forward */
                    col=CURSOR_POS_COL(page);
                    row=CURSOR_POS_ROW(page);
                    tempdata=(ansi.data[0]? ansi.data[0] : 1);
                    if(tempdata + static_cast<Bitu>(col) >= ansi.ncols) 
                    { col = ansi.ncols - 1;} 
                    else	{ col += tempdata;}
                    Real_INT10_SetCursorPos(row,col,page);
                    ClearAnsi();
                    break;
                case 'D': /*Cursor Backward  */
                    col=CURSOR_POS_COL(page);
                    row=CURSOR_POS_ROW(page);
                    tempdata=(ansi.data[0]? ansi.data[0] : 1);
                    if(tempdata > col) {col = 0;}
                    else { col -= tempdata;}
                    Real_INT10_SetCursorPos(row,col,page);
                    ClearAnsi();
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
                    INT10_WriteChar(' ',ansi.attr,page,ansi.ncols-col,true); //Real_WriteChar(ansi.ncols-col,row,page,' ',ansi.attr,true);

                    //for(i = col;i<(Bitu) ansi.ncols; i++) INT10_TeletypeOutputAttr(' ',ansi.attr,true);
                    Real_INT10_SetCursorPos(row,col,page);
                    ClearAnsi();
                    break;
                case 'M': /* delete line (NANSI) */
                    col = CURSOR_POS_COL(page);
                    row = CURSOR_POS_ROW(page);
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

bool device_CON::Seek(Bit32u * pos,Bit32u type) {
	// seek is valid
	*pos = 0;
	return true;
}

bool device_CON::Close() {
	return true;
}

extern bool dos_con_use_int16_to_detect_input;

Bit16u device_CON::GetInformation(void) {
	if (dos_con_use_int16_to_detect_input || IS_PC98_ARCH) {
		Bit16u ret = 0x80D3; /* No Key Available */

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

		Bitu saved_ax = reg_ax;

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
		Bit16u head=mem_readw(BIOS_KEYBOARD_BUFFER_HEAD);
		Bit16u tail=mem_readw(BIOS_KEYBOARD_BUFFER_TAIL);

		if ((head==tail) && !readcache) return 0x80D3;	/* No Key Available */
		if (readcache || real_readw(0x40,head)) return 0x8093;		/* Key Available */

		/* remove the zero from keyboard buffer */
		Bit16u start=mem_readw(BIOS_KEYBOARD_BUFFER_START);
		Bit16u end	=mem_readw(BIOS_KEYBOARD_BUFFER_END);
		head+=2;
		if (head>=end) head=start;
		mem_writew(BIOS_KEYBOARD_BUFFER_HEAD,head);
	}

	return 0x80D3; /* No Key Available */
}

device_CON::device_CON() {
	SetName("CON");
	readcache=0;
	lastwrite=0;
	ansi.enabled=false;
	ansi.attr=0x7;
	ansi.ncols=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS); //should be updated once set/reset mode is implemented
	ansi.nrows=real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS) + 1;
	ansi.saverow=0;
	ansi.savecol=0;
	ansi.warned=false;
	ClearAnsi();
}

void device_CON::ClearAnsi(void){
	for(Bit8u i=0; i<NUMBER_ANSI_DATA;i++) ansi.data[i]=0;
    ansi.pc98rab=false;
	ansi.esc=false;
	ansi.sci=false;
	ansi.numberofarg=0;
}

