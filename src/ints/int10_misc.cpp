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
#include "mem.h"
#include "inout.h"
#include "int10.h"



#pragma pack(1)
struct Dynamic_Functionality {
	RealPt static_table;		/* 00h   DWORD  address of static functionality table */
	Bit8u cur_mode;				/* 04h   BYTE   video mode in effect */
	Bit16u num_cols;			/* 05h   WORD   number of columns */
	Bit16u regen_size;			/* 07h   WORD   length of regen buffer in bytes */
	Bit16u regen_start;			/* 09h   WORD   starting address of regen buffer*/
	Bit16u cursor_pos[8];		/* 0Bh   WORD   cursor position for page 0-7 */
	Bit16u cursor_type;			/* 1Bh   WORD   cursor type */
	Bit8u active_page;			/* 1Dh   BYTE   active display page */
	Bit16u crtc_address;		/* 1Eh   WORD   CRTC port address */
	Bit8u reg_3x8;				/* 20h   BYTE   current setting of register (3?8) */
	Bit8u reg_3x9;				/* 21h   BYTE   current setting of register (3?9) */
	Bit8u num_rows;				/* 22h   BYTE   number of rows */
	Bit16u bytes_per_char;		/* 23h   WORD   bytes/character */
	Bit8u dcc;					/* 25h   BYTE   display combination code of active display */
	Bit8u dcc_alternate;		/* 26h   BYTE   DCC of alternate display */
	Bit16u num_colors;			/* 27h   WORD   number of colors supported in current mode */
	Bit8u num_pages;			/* 29h   BYTE   number of pages supported in current mode */
	Bit8u num_scanlines;		/* 2Ah   BYTE   number of scan lines active mode (0,1,2,3) = (200,350,400,480) */
	Bit8u pri_char_block;		/* 2Bh   BYTE   primary character block */
	Bit8u sec_char_block;		/* 2Ch   BYTE   secondary character block */
	Bit8u misc_flags;			/* 2Dh   BYTE   miscellaneous flags
									bit 0 all modes on all displays on
									1 grey summing on
									2 monochrome display attached
									3 default palette loading disabled
									4 cursor emulation enabled
									5 0 = intensity; 1 = blinking
									6 PS/2 P70 plasma display (without 9-dot wide font) active
									7 reserved
								*/
	Bit8u reserved1[3];			/* 2Eh  3 BYTEs reserved (00h) */
	Bit8u vid_mem;				/* 31h   BYTE   video memory available 00h = 64K, 01h = 128K, 02h = 192K, 03h = 256K */
	Bit8u savep_state_flag;		/* 32h   BYTE   save pointer state flags 
									bit 0 512 character set active
									1 dynamic save area present
									2 alpha font override active
									3 graphics font override active
									4 palette override active
									5 DCC override active
									6 reserved
									7 reserved
								*/
	Bit8u reserved2[13];		/*  33h 13 BYTEs reserved (00h) */
} GCC_ATTRIBUTE(packed);
#pragma pack()

void INT10_DisplayCombinationCode(Bit16u * dcc,bool set) {
    // FIXME
    if (machine == MCH_MCGA) {
        *dcc = 0x0C; // MCGA color analogue display
        return;
    }

	Bit8u index=0xff;
	Bit16u dccentry=0xffff;
	// walk the tables...
	RealPt vsavept=real_readd(BIOSMEM_SEG,BIOSMEM_VS_POINTER);
	RealPt svstable=real_readd(RealSeg(vsavept),RealOff(vsavept)+0x10);
	if (svstable) {
		RealPt dcctable=real_readd(RealSeg(svstable),RealOff(svstable)+0x02);
		Bit8u entries=real_readb(RealSeg(dcctable),RealOff(dcctable)+0x00);
		if (set) {
			if (entries) {
				Bit16u swap=(*dcc<<8)|(*dcc>>8);
				// search for the index in the dcc table
				for (Bit8u entry=0; entry<entries; entry++) {
					dccentry=real_readw(RealSeg(dcctable),RealOff(dcctable)+0x04+entry*2);
					if ((dccentry==*dcc) || (dccentry==swap)) {
						index=entry;
						break;
					}
				}
			}
		} else {
			index=real_readb(BIOSMEM_SEG,BIOSMEM_DCC_INDEX);
			// check if index within range
			if (index<entries) {
				dccentry=real_readw(RealSeg(dcctable),RealOff(dcctable)+0x04+index*2);
				if ((dccentry&0xff)==0) dccentry>>=8;
				else if (dccentry>>8) {
					Bit16u cfg_mono=((real_readw(BIOSMEM_SEG,BIOSMEM_INITIAL_MODE)&0x30)==0x30)?1:0;
					if (cfg_mono^(dccentry&1)) dccentry=(dccentry<<8)|(dccentry>>8);
				}
			}
		}
	}
	if (set) real_writeb(BIOSMEM_SEG,BIOSMEM_DCC_INDEX,index);
	else *dcc=dccentry;
}

void INT10_GetFuncStateInformation(PhysPt save) {
	/* set static state pointer */
	mem_writed(save,int10.rom.static_state);
	/* Copy BIOS Segment areas */
	Bit16u i;

	/* First area in Bios Seg */
	for (i=0;i<0x1e;i++) {
		mem_writeb(save+0x4+i,real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE+i));
	}
	/* Second area */
	mem_writeb(save+0x22,real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1);
	for (i=1;i<3;i++) {
		mem_writeb(save+0x22+i,real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS+i));
	}
	/* Zero out rest of block */
	for (i=0x25;i<0x40;i++) mem_writeb(save+i,0);
	/* DCC */
	Bit16u dcc=0;
	INT10_DisplayCombinationCode(&dcc,false);
	mem_writew(save+0x25,dcc);

	Bit16u col_count=0;
	switch (CurMode->type) {
	case M_TEXT:
		if (CurMode->mode==0x7) col_count=1; else col_count=16;break; 
	case M_CGA2:
		col_count=2;break;
	case M_CGA4:
		col_count=4;break;
	case M_EGA:
		if (CurMode->mode==0x11 || CurMode->mode==0x0f) 
			col_count=2; 
		else 
			col_count=16;
		break; 
	case M_VGA:
		col_count=256;break;
	default:
		LOG(LOG_INT10,LOG_ERROR)("Get Func State illegal mode type %d",CurMode->type);
	}
	/* Colour count */
	mem_writew(save+0x27,col_count);
	/* Page count */
	mem_writeb(save+0x29,(Bit8u)CurMode->ptotal);
	/* scan lines */
	switch (CurMode->sheight) {
	case 200:
		mem_writeb(save+0x2a,0);break;
	case 350:
		mem_writeb(save+0x2a,1);break;
	case 400:
		mem_writeb(save+0x2a,2);break;
	case 480:
		mem_writeb(save+0x2a,3);break;
	}
	/* misc flags */
	if (CurMode->type==M_TEXT) mem_writeb(save+0x2d,0x21);
	else mem_writeb(save+0x2d,0x01);
	/* Video Memory available */
	mem_writeb(save+0x31,3);
}

RealPt INT10_EGA_RIL_GetVersionPt(void) {
	/* points to a graphics ROM location at the moment
	   as checks test for bx!=0 only */
	return RealMake(0xc000,0x30);
}

static void EGA_RIL(Bit16u dx, Bit16u& port, Bit16u& regs) {
	port = 0;
	regs = 0; //if nul is returned it's a single register port
	switch(dx) {
	case 0x00: /* CRT Controller (25 reg) 3B4h mono modes, 3D4h color modes */
		port = real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
		regs = 25;
		break;
	case 0x08: /* Sequencer (5 registers) 3C4h */
		port = 0x3C4;
		regs = 5;
		break;
	case 0x10: /* Graphics Controller (9 registers) 3CEh */
		port = 0x3CE;
		regs = 9;
		break;
	case 0x18: /* Attribute Controller (20 registers) 3C0h */
		port = 0x3c0;
		regs = 20;
		break;
	case 0x20: /* Miscellaneous Output register 3C2h */
		port = 0x3C2;
		break;
	case 0x28: /* Feature Control register (3BAh mono modes, 3DAh color modes) */
		port = real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS) + 6u;
		break;
	case 0x30: /* Graphics 1 Position register 3CCh */
		port = 0x3CC;
		break;
	case 0x38: /* Graphics 2 Position register 3CAh */
		port = 0x3CA;
		break;
	default:
		LOG(LOG_INT10,LOG_ERROR)("unknown RIL port selection %X",dx);
		break;
	}
}

void INT10_EGA_RIL_ReadRegister(Bit8u & bl, Bit16u dx) {
	Bit16u port = 0;
	Bit16u regs = 0;
	EGA_RIL(dx,port,regs);
	if(regs == 0) {
		if(port) bl = IO_Read(port);
	} else {
		if(port == 0x3c0) IO_Read(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS) + 6u);
		IO_Write(port,bl);
		bl = IO_Read(port+1u);
		if(port == 0x3c0) IO_Read(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS) + 6u);
		LOG(LOG_INT10,LOG_NORMAL)("EGA RIL read used with multi-reg");
	}
}

void INT10_EGA_RIL_WriteRegister(Bit8u & bl, Bit8u bh, Bit16u dx) {
	Bit16u port = 0;
	Bit16u regs = 0;
	EGA_RIL(dx,port,regs);
	if(regs == 0) {
		if(port) IO_Write(port,bl);
	} else {
		if(port == 0x3c0) {
			IO_Read(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS) + 6u);
			IO_Write(port,bl);
			IO_Write(port,bh);
		} else {
			IO_Write(port,bl);
			IO_Write(port+1u,bh);
		}
		bl = bh;//Not sure
		LOG(LOG_INT10,LOG_NORMAL)("EGA RIL write used with multi-reg");
	}
}

void INT10_EGA_RIL_ReadRegisterRange(Bit8u ch, Bit8u cl, Bit16u dx, PhysPt dst) {
	Bit16u port = 0;
	Bit16u regs = 0;
	EGA_RIL(dx,port,regs);
	if(regs == 0) {
		LOG(LOG_INT10,LOG_ERROR)("EGA RIL range read with port %x called",(int)port);
	} else {
		if(ch<regs) {
			if ((Bitu)ch+cl>regs) cl=(Bit8u)(regs-ch);
			for (Bitu i=0; i<cl; i++) {
				if(port == 0x3c0) IO_Read(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS) + 6u);
				IO_Write(port,(Bit8u)(ch+i));
				mem_writeb(dst++,IO_Read(port+1u));
			}
			if(port == 0x3c0) IO_Read(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS) + 6u);
		} else LOG(LOG_INT10,LOG_ERROR)("EGA RIL range read from %x for invalid register %x",(int)port,(int)ch);
	}
}

void INT10_EGA_RIL_WriteRegisterRange(Bit8u ch, Bit8u cl, Bit16u dx, PhysPt src) {
	Bit16u port = 0;
	Bit16u regs = 0;
	EGA_RIL(dx,port,regs);
	if(regs == 0) {
		LOG(LOG_INT10,LOG_ERROR)("EGA RIL range write called with port %x",(int)port);
	} else {
		if(ch<regs) {
			if ((Bitu)ch+cl>regs) cl=(Bit8u)(regs-ch);
			if(port == 0x3c0) {
				IO_Read(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS) + 6u);
				for (Bitu i=0; i<cl; i++) {
					IO_Write(port,(Bit8u)(ch+i));
					IO_Write(port,mem_readb(src++));
				}
			} else {
				for (Bitu i=0; i<cl; i++) {
					IO_Write(port,(Bit8u)(ch+i));
					IO_Write(port+1u,mem_readb(src++));
				}
			}
		} else LOG(LOG_INT10,LOG_ERROR)("EGA RIL range write to %x with invalid register %x",(int)port,(int)ch);
	}
}

/* register sets are of the form
   offset 0 (word): group index
   offset 2 (byte): register number (0 for single registers, ignored)
   offset 3 (byte): register value (return value when reading)
*/
void INT10_EGA_RIL_ReadRegisterSet(Bit16u cx, PhysPt tbl) {
	/* read cx register sets */
	for (Bit16u i=0; i<cx; i++) {
		Bit8u vl=mem_readb(tbl+2);
		INT10_EGA_RIL_ReadRegister(vl, mem_readw(tbl));
		mem_writeb(tbl+3, vl);
		tbl+=4;
	}
}

void INT10_EGA_RIL_WriteRegisterSet(Bit16u cx, PhysPt tbl) {
	/* write cx register sets */
	Bit16u port = 0;
	Bit16u regs = 0;
	for (Bit16u i=0; i<cx; i++) {
		EGA_RIL(mem_readw(tbl),port,regs);
		Bit8u vl=mem_readb(tbl+3);
		if(regs == 0) {
			if(port) IO_Write(port,vl);
		} else {
			Bit8u idx=mem_readb(tbl+2);
			if(port == 0x3c0) {
				IO_Read(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS) + 6u);
				IO_Write(port,idx);
				IO_Write(port,vl);
			} else {
				IO_Write(port,idx);
				IO_Write(port+1u,vl);
			}
		}
		tbl+=4;
	}
}
