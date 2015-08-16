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


#include "dosbox.h"
#include "mem.h"
#include "inout.h"
#include "int10.h"

Bitu INT10_VideoState_GetSize(Bitu state) {
	// state: bit0=hardware, bit1=bios data, bit2=color regs/dac state
	if ((state&7)==0) return 0;

	Bitu size=0x20;
	if (state&1) size+=0x46;
	if (state&2) size+=0x3a;
	if (state&4) size+=0x303;
	if ((svgaCard==SVGA_S3Trio) && (state&8)) size+=0x43;
	if (size!=0) size=(size-1)/64+1;
	return size;
}

bool INT10_VideoState_Save(Bitu state,RealPt buffer) {
	Bitu ct;
	if ((state&7)==0) return false;

	Bitu base_seg=RealSeg(buffer);
	Bitu base_dest=RealOff(buffer)+0x20;

	if (state&1)  {
		real_writew(base_seg,RealOff(buffer),base_dest);

		Bit16u crt_reg=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
		real_writew(base_seg,base_dest+0x40,crt_reg);

		real_writeb(base_seg,base_dest+0x00,IO_ReadB(0x3c4));
		real_writeb(base_seg,base_dest+0x01,IO_ReadB(0x3d4));
		real_writeb(base_seg,base_dest+0x02,IO_ReadB(0x3ce));
		IO_ReadB(crt_reg+6);
		real_writeb(base_seg,base_dest+0x03,IO_ReadB(0x3c0));
		real_writeb(base_seg,base_dest+0x04,IO_ReadB(0x3ca));

		// sequencer
		for (ct=1; ct<5; ct++) {
			IO_WriteB(0x3c4,ct);
			real_writeb(base_seg,base_dest+0x04+ct,IO_ReadB(0x3c5));
		}

		real_writeb(base_seg,base_dest+0x09,IO_ReadB(0x3cc));

		// crt controller
		for (ct=0; ct<0x19; ct++) {
			IO_WriteB(crt_reg,ct);
			real_writeb(base_seg,base_dest+0x0a+ct,IO_ReadB(crt_reg+1));
		}

		// attr registers
		for (ct=0; ct<4; ct++) {
			IO_ReadB(crt_reg+6);
			IO_WriteB(0x3c0,0x10+ct);
			real_writeb(base_seg,base_dest+0x33+ct,IO_ReadB(0x3c1));
		}

		// graphics registers
		for (ct=0; ct<9; ct++) {
			IO_WriteB(0x3ce,ct);
			real_writeb(base_seg,base_dest+0x37+ct,IO_ReadB(0x3cf));
		}

		// save some registers
		IO_WriteB(0x3c4,2);
		Bit8u crtc_2=IO_ReadB(0x3c5);
		IO_WriteB(0x3c4,4);
		Bit8u crtc_4=IO_ReadB(0x3c5);
		IO_WriteB(0x3ce,6);
		Bit8u gfx_6=IO_ReadB(0x3cf);
		IO_WriteB(0x3ce,5);
		Bit8u gfx_5=IO_ReadB(0x3cf);
		IO_WriteB(0x3ce,4);
		Bit8u gfx_4=IO_ReadB(0x3cf);

		// reprogram for full access to plane latches
		IO_WriteW(0x3c4,0x0f02);
		IO_WriteW(0x3c4,0x0704);
		IO_WriteW(0x3ce,0x0406);
		IO_WriteW(0x3ce,0x0105);
		mem_writeb(0xaffff,0);

		for (ct=0; ct<4; ct++) {
			IO_WriteW(0x3ce,0x0004+ct*0x100);
			real_writeb(base_seg,base_dest+0x42+ct,mem_readb(0xaffff));
		}

		// restore registers
		IO_WriteW(0x3ce,0x0004|(gfx_4<<8));
		IO_WriteW(0x3ce,0x0005|(gfx_5<<8));
		IO_WriteW(0x3ce,0x0006|(gfx_6<<8));
		IO_WriteW(0x3c4,0x0004|(crtc_4<<8));
		IO_WriteW(0x3c4,0x0002|(crtc_2<<8));

		for (ct=0; ct<0x10; ct++) {
			IO_ReadB(crt_reg+6);
			IO_WriteB(0x3c0,ct);
			real_writeb(base_seg,base_dest+0x23+ct,IO_ReadB(0x3c1));
		}
		IO_WriteB(0x3c0,0x20);

		base_dest+=0x46;
	}

	if (state&2)  {
		real_writew(base_seg,RealOff(buffer)+2,base_dest);

		real_writeb(base_seg,base_dest+0x00,mem_readb(0x410)&0x30);
		for (ct=0; ct<0x1e; ct++) {
			real_writeb(base_seg,base_dest+0x01+ct,mem_readb(0x449+ct));
		}
		for (ct=0; ct<0x07; ct++) {
			real_writeb(base_seg,base_dest+0x1f+ct,mem_readb(0x484+ct));
		}
		real_writed(base_seg,base_dest+0x26,mem_readd(0x48a));
		real_writed(base_seg,base_dest+0x2a,mem_readd(0x14));	// int 5
		real_writed(base_seg,base_dest+0x2e,mem_readd(0x74));	// int 1d
		real_writed(base_seg,base_dest+0x32,mem_readd(0x7c));	// int 1f
		real_writed(base_seg,base_dest+0x36,mem_readd(0x10c));	// int 43

		base_dest+=0x3a;
	}

	if (state&4)  {
		real_writew(base_seg,RealOff(buffer)+4,base_dest);

		Bit16u crt_reg=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);

		IO_ReadB(crt_reg+6);
		IO_WriteB(0x3c0,0x14);
		real_writeb(base_seg,base_dest+0x303,IO_ReadB(0x3c1));

		Bitu dac_state=IO_ReadB(0x3c7)&1;
		Bitu dac_windex=IO_ReadB(0x3c8);
		if (dac_state!=0) dac_windex--;
		real_writeb(base_seg,base_dest+0x000,dac_state);
		real_writeb(base_seg,base_dest+0x001,dac_windex);
		real_writeb(base_seg,base_dest+0x002,IO_ReadB(0x3c6));

		for (ct=0; ct<0x100; ct++) {
			IO_WriteB(0x3c7,ct);
			real_writeb(base_seg,base_dest+0x003+ct*3+0,IO_ReadB(0x3c9));
			real_writeb(base_seg,base_dest+0x003+ct*3+1,IO_ReadB(0x3c9));
			real_writeb(base_seg,base_dest+0x003+ct*3+2,IO_ReadB(0x3c9));
		}

		IO_ReadB(crt_reg+6);
		IO_WriteB(0x3c0,0x20);

		base_dest+=0x303;
	}

	if ((svgaCard==SVGA_S3Trio) && (state&8))  {
		real_writew(base_seg,RealOff(buffer)+6,base_dest);

		Bit16u crt_reg=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);

		IO_WriteB(0x3c4,0x08);
//		Bitu seq_8=IO_ReadB(0x3c5);
		IO_ReadB(0x3c5);
//		real_writeb(base_seg,base_dest+0x00,IO_ReadB(0x3c5));
		IO_WriteB(0x3c5,0x06);	// unlock s3-specific registers

		// sequencer
		for (ct=0; ct<0x13; ct++) {
			IO_WriteB(0x3c4,0x09+ct);
			real_writeb(base_seg,base_dest+0x00+ct,IO_ReadB(0x3c5));
		}

		// unlock s3-specific registers
		IO_WriteW(crt_reg,0x4838);
		IO_WriteW(crt_reg,0xa539);

		// crt controller
		Bitu ct_dest=0x13;
		for (ct=0; ct<0x40; ct++) {
			if ((ct==0x4a-0x30) || (ct==0x4b-0x30)) {
				IO_WriteB(crt_reg,0x45);
				IO_ReadB(crt_reg+1);
				IO_WriteB(crt_reg,0x30+ct);
				real_writeb(base_seg,base_dest+(ct_dest++),IO_ReadB(crt_reg+1));
				real_writeb(base_seg,base_dest+(ct_dest++),IO_ReadB(crt_reg+1));
				real_writeb(base_seg,base_dest+(ct_dest++),IO_ReadB(crt_reg+1));
			} else {
				IO_WriteB(crt_reg,0x30+ct);
				real_writeb(base_seg,base_dest+(ct_dest++),IO_ReadB(crt_reg+1));
			}
		}
	}
	return true;
}

bool INT10_VideoState_Restore(Bitu state,RealPt buffer) {
	Bitu ct;
	if ((state&7)==0) return false;

	Bit16u base_seg=RealSeg(buffer);
	Bit16u base_dest;

	if (state&1)  {
		base_dest=real_readw(base_seg,RealOff(buffer));
		Bit16u crt_reg=real_readw(base_seg,base_dest+0x40);

		// reprogram for full access to plane latches
		IO_WriteW(0x3c4,0x0704);
		IO_WriteW(0x3ce,0x0406);
		IO_WriteW(0x3ce,0x0005);

		IO_WriteW(0x3c4,0x0002);
		mem_writeb(0xaffff,real_readb(base_seg,base_dest+0x42));
		IO_WriteW(0x3c4,0x0102);
		mem_writeb(0xaffff,real_readb(base_seg,base_dest+0x43));
		IO_WriteW(0x3c4,0x0202);
		mem_writeb(0xaffff,real_readb(base_seg,base_dest+0x44));
		IO_WriteW(0x3c4,0x0402);
		mem_writeb(0xaffff,real_readb(base_seg,base_dest+0x45));
		IO_WriteW(0x3c4,0x0f02);
		mem_readb(0xaffff);

		IO_WriteW(0x3c4,0x0100);

		// sequencer
		for (ct=1; ct<5; ct++) {
			IO_WriteW(0x3c4,ct+(real_readb(base_seg,base_dest+0x04+ct)<<8));
		}

		IO_WriteB(0x3c2,real_readb(base_seg,base_dest+0x09));
		IO_WriteW(0x3c4,0x0300);
		IO_WriteW(crt_reg,0x0011);

		// crt controller
		for (ct=0; ct<0x19; ct++) {
			IO_WriteW(crt_reg,ct+(real_readb(base_seg,base_dest+0x0a+ct)<<8));
		}

		IO_ReadB(crt_reg+6);
		// attr registers
		for (ct=0; ct<4; ct++) {
			IO_WriteB(0x3c0,0x10+ct);
			IO_WriteB(0x3c0,real_readb(base_seg,base_dest+0x33+ct));
		}

		// graphics registers
		for (ct=0; ct<9; ct++) {
			IO_WriteW(0x3ce,ct+(real_readb(base_seg,base_dest+0x37+ct)<<8));
		}

		IO_WriteB(crt_reg+6,real_readb(base_seg,base_dest+0x04));
		IO_ReadB(crt_reg+6);

		// attr registers
		for (ct=0; ct<0x10; ct++) {
			IO_WriteB(0x3c0,ct);
			IO_WriteB(0x3c0,real_readb(base_seg,base_dest+0x23+ct));
		}

		IO_WriteB(0x3c4,real_readb(base_seg,base_dest+0x00));
		IO_WriteB(0x3d4,real_readb(base_seg,base_dest+0x01));
		IO_WriteB(0x3ce,real_readb(base_seg,base_dest+0x02));
		IO_ReadB(crt_reg+6);
		IO_WriteB(0x3c0,real_readb(base_seg,base_dest+0x03));
	}

	if (state&2)  {
		base_dest=real_readw(base_seg,RealOff(buffer)+2);

		mem_writeb(0x410,(mem_readb(0x410)&0xcf) | real_readb(base_seg,base_dest+0x00));
		for (ct=0; ct<0x1e; ct++) {
			mem_writeb(0x449+ct,real_readb(base_seg,base_dest+0x01+ct));
		}
		for (ct=0; ct<0x07; ct++) {
			mem_writeb(0x484+ct,real_readb(base_seg,base_dest+0x1f+ct));
		}
		mem_writed(0x48a,real_readd(base_seg,base_dest+0x26));
		mem_writed(0x14,real_readd(base_seg,base_dest+0x2a));	// int 5
		mem_writed(0x74,real_readd(base_seg,base_dest+0x2e));	// int 1d
		mem_writed(0x7c,real_readd(base_seg,base_dest+0x32));	// int 1f
		mem_writed(0x10c,real_readd(base_seg,base_dest+0x36));	// int 43
	}

	if (state&4)  {
		base_dest=real_readw(base_seg,RealOff(buffer)+4);

		Bit16u crt_reg=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);

		IO_WriteB(0x3c6,real_readb(base_seg,base_dest+0x002));

		for (ct=0; ct<0x100; ct++) {
			IO_WriteB(0x3c8,ct);
			IO_WriteB(0x3c9,real_readb(base_seg,base_dest+0x003+ct*3+0));
			IO_WriteB(0x3c9,real_readb(base_seg,base_dest+0x003+ct*3+1));
			IO_WriteB(0x3c9,real_readb(base_seg,base_dest+0x003+ct*3+2));
		}

		IO_ReadB(crt_reg+6);
		IO_WriteB(0x3c0,0x14);
		IO_WriteB(0x3c0,real_readb(base_seg,base_dest+0x303));

		Bitu dac_state=real_readb(base_seg,base_dest+0x000);
		if (dac_state==0) {
			IO_WriteB(0x3c8,real_readb(base_seg,base_dest+0x001));
		} else {
			IO_WriteB(0x3c7,real_readb(base_seg,base_dest+0x001));
		}
	}

	if ((svgaCard==SVGA_S3Trio) && (state&8))  {
		base_dest=real_readw(base_seg,RealOff(buffer)+6);

		Bit16u crt_reg=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);

		Bitu seq_idx=IO_ReadB(0x3c4);
		IO_WriteB(0x3c4,0x08);
//		Bitu seq_8=IO_ReadB(0x3c5);
		IO_ReadB(0x3c5);
//		real_writeb(base_seg,base_dest+0x00,IO_ReadB(0x3c5));
		IO_WriteB(0x3c5,0x06);	// unlock s3-specific registers

		// sequencer
		for (ct=0; ct<0x13; ct++) {
			IO_WriteW(0x3c4,(0x09+ct)+(real_readb(base_seg,base_dest+0x00+ct)<<8));
		}
		IO_WriteB(0x3c4,seq_idx);

//		Bitu crtc_idx=IO_ReadB(0x3d4);

		// unlock s3-specific registers
		IO_WriteW(crt_reg,0x4838);
		IO_WriteW(crt_reg,0xa539);

		// crt controller
		Bitu ct_dest=0x13;
		for (ct=0; ct<0x40; ct++) {
			if ((ct==0x4a-0x30) || (ct==0x4b-0x30)) {
				IO_WriteB(crt_reg,0x45);
				IO_ReadB(crt_reg+1);
				IO_WriteB(crt_reg,0x30+ct);
				IO_WriteB(crt_reg,real_readb(base_seg,base_dest+(ct_dest++)));
			} else {
				IO_WriteW(crt_reg,(0x30+ct)+(real_readb(base_seg,base_dest+(ct_dest++))<<8));
			}
		}

		// mmio
/*		IO_WriteB(crt_reg,0x40);
		Bitu sysval1=IO_ReadB(crt_reg+1);
		IO_WriteB(crt_reg+1,sysval|1);
		IO_WriteB(crt_reg,0x53);
		Bitu sysva2=IO_ReadB(crt_reg+1);
		IO_WriteB(crt_reg+1,sysval2|0x10);

		real_writew(0xa000,0x8128,0xffff);

		IO_WriteB(crt_reg,0x40);
		IO_WriteB(crt_reg,sysval1);
		IO_WriteB(crt_reg,0x53);
		IO_WriteB(crt_reg,sysval2);
		IO_WriteB(crt_reg,crtc_idx); */
	}

	return true;
}
