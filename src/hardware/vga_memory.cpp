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


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dosbox.h"
#include "mem.h"
#include "vga.h"
#include "paging.h"
#include "pic.h"
#include "inout.h"
#include "setup.h"
#include "cpu.h"

#ifndef C_VGARAM_CHECKED
#define C_VGARAM_CHECKED 1
#endif

#if C_VGARAM_CHECKED
// Checked linear offset
#define CHECKED(v) ((v)&(vga.vmemwrap-1))
// Checked planar offset (latched access)
#define CHECKED2(v) ((v)&((vga.vmemwrap>>2)-1))
#else
#define CHECKED(v) (v)
#define CHECKED2(v) (v)
#endif

#define CHECKED3(v) ((v)&(vga.vmemwrap-1))
#define CHECKED4(v) ((v)&((vga.vmemwrap>>2)-1))

#define TANDY_VIDBASE(_X_)  &MemBase[ 0x80000 + (_X_)]

/* how much delay to add to VGA memory I/O in nanoseconds */
int vga_memio_delay_ns = 1000;

void VGAMEM_USEC_read_delay() {
	if (vga_memio_delay_ns > 0) {
		Bits delaycyc = (CPU_CycleMax * vga_memio_delay_ns) / 1000000;
//		if(GCC_UNLIKELY(CPU_Cycles < 3*delaycyc)) delaycyc = 0; //Else port acces will set cycles to 0. which might trigger problem with games which read 16 bit values
		CPU_Cycles -= delaycyc;
		CPU_IODelayRemoved += delaycyc;
	}
}

void VGAMEM_USEC_write_delay() {
	if (vga_memio_delay_ns > 0) {
		Bits delaycyc = (CPU_CycleMax * vga_memio_delay_ns * 3) / (1000000 * 4);
//		if(GCC_UNLIKELY(CPU_Cycles < 3*delaycyc)) delaycyc = 0; //Else port acces will set cycles to 0. which might trigger problem with games which read 16 bit values
		CPU_Cycles -= delaycyc;
		CPU_IODelayRemoved += delaycyc;
	}
}

template <class Size>
static INLINE void hostWrite(HostPt off, Bitu val) {
	if ( sizeof( Size ) == 1)
		host_writeb( off, (Bit8u)val );
	else if ( sizeof( Size ) == 2)
		host_writew( off, (Bit16u)val );
	else if ( sizeof( Size ) == 4)
		host_writed( off, (Bit32u)val );
}

template <class Size>
static INLINE Bitu  hostRead(HostPt off ) {
	if ( sizeof( Size ) == 1)
		return host_readb( off );
	else if ( sizeof( Size ) == 2)
		return host_readw( off );
	else if ( sizeof( Size ) == 4)
		return host_readd( off );
	return 0;
}


void VGA_MapMMIO(void);
//Nice one from DosEmu
INLINE static Bit32u RasterOp(Bit32u input,Bit32u mask) {
	switch (vga.config.raster_op) {
	case 0x00:	/* None */
		return (input & mask) | (vga.latch.d & ~mask);
	case 0x01:	/* AND */
		return (input | ~mask) & vga.latch.d;
	case 0x02:	/* OR */
		return (input & mask) | vga.latch.d;
	case 0x03:	/* XOR */
		return (input & mask) ^ vga.latch.d;
	};
	return 0;
}

INLINE static Bit32u ModeOperation(Bit8u val) {
	Bit32u full;
	switch (vga.config.write_mode) {
	case 0x00:
		// Write Mode 0: In this mode, the host data is first rotated as per the Rotate Count field, then the Enable Set/Reset mechanism selects data from this or the Set/Reset field. Then the selected Logical Operation is performed on the resulting data and the data in the latch register. Then the Bit Mask field is used to select which bits come from the resulting data and which come from the latch register. Finally, only the bit planes enabled by the Memory Plane Write Enable field are written to memory. 
		val=((val >> vga.config.data_rotate) | (val << (8-vga.config.data_rotate)));
		full=ExpandTable[val];
		full=(full & vga.config.full_not_enable_set_reset) | vga.config.full_enable_and_set_reset; 
		full=RasterOp(full,vga.config.full_bit_mask);
		break;
	case 0x01:
		// Write Mode 1: In this mode, data is transferred directly from the 32 bit latch register to display memory, affected only by the Memory Plane Write Enable field. The host data is not used in this mode. 
		full=vga.latch.d;
		break;
	case 0x02:
		//Write Mode 2: In this mode, the bits 3-0 of the host data are replicated across all 8 bits of their respective planes. Then the selected Logical Operation is performed on the resulting data and the data in the latch register. Then the Bit Mask field is used to select which bits come from the resulting data and which come from the latch register. Finally, only the bit planes enabled by the Memory Plane Write Enable field are written to memory. 
		full=RasterOp(FillTable[val&0xF],vga.config.full_bit_mask);
		break;
	case 0x03:
		// Write Mode 3: In this mode, the data in the Set/Reset field is used as if the Enable Set/Reset field were set to 1111b. Then the host data is first rotated as per the Rotate Count field, then logical ANDed with the value of the Bit Mask field. The resulting value is used on the data obtained from the Set/Reset field in the same way that the Bit Mask field would ordinarily be used. to select which bits come from the expansion of the Set/Reset field and which come from the latch register. Finally, only the bit planes enabled by the Memory Plane Write Enable field are written to memory.
		val=((val >> vga.config.data_rotate) | (val << (8-vga.config.data_rotate)));
		full=RasterOp(vga.config.full_set_reset,ExpandTable[val] & vga.config.full_bit_mask);
		break;
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:Unsupported write mode %d",vga.config.write_mode);
		full=0;
		break;
	}
	return full;
}

/* Gonna assume that whoever maps vga memory, maps it on 32/64kb boundary */

#define VGA_PAGES		(128/4)
#define VGA_PAGE_A0		(0xA0000/4096)
#define VGA_PAGE_B0		(0xB0000/4096)
#define VGA_PAGE_B8		(0xB8000/4096)

static struct {
	Bitu base, mask;
} vgapages;
	
class VGA_UnchainedRead_Handler : public PageHandler {
public:
	VGA_UnchainedRead_Handler(Bitu flags) : PageHandler(flags) {}
	Bitu readHandler(PhysPt start) {
		PhysPt memstart = start;
		unsigned char bplane;

		if (vga.gfx.miscellaneous&2) /* Odd/Even mode */
			memstart &= ~1;

		vga.latch.d=((Bit32u*)vga.mem.linear)[memstart];
		switch (vga.config.read_mode) {
			case 0:
				bplane = vga.config.read_map_select;
				/* NTS: We check the sequencer AND the GC to know whether we mask the bitplane line this,
				 *      even though in TEXT mode we only check the sequencer. Without this extra check,
				 *      Windows 95 and Windows 3.1 will exhibit glitches in the standard VGA 640x480x16
				 *      planar mode */
				if (!(vga.seq.memory_mode&4) && (vga.gfx.miscellaneous&2)) /* FIXME: How exactly do SVGA cards determine this? */
					bplane = (bplane & ~1) + (start & 1); /* FIXME: Is this what VGA cards do? It makes sense to me */
				return (vga.latch.b[bplane]);
			case 1:
				VGA_Latch templatch;
				templatch.d=(vga.latch.d & FillTable[vga.config.color_dont_care]) ^ FillTable[vga.config.color_compare & vga.config.color_dont_care];
				return (Bit8u)~(templatch.b[0] | templatch.b[1] | templatch.b[2] | templatch.b[3]);
		}
		return 0;
	}
public:
	Bitu readb(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED2(addr);
		return readHandler(addr);
	}
	Bitu readw(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED2(addr);
		Bitu ret = (readHandler(addr+0) << 0);
		ret     |= (readHandler(addr+1) << 8);
		return  ret;
	}
	Bitu readd(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED2(addr);
		Bitu ret = (readHandler(addr+0) << 0);
		ret     |= (readHandler(addr+1) << 8);
		ret     |= (readHandler(addr+2) << 16);
		ret     |= (readHandler(addr+3) << 24);
		return ret;
	}
};

class VGA_ChainedEGA_Handler : public PageHandler {
public:
	Bitu readHandler(PhysPt addr) {
		return vga.mem.linear[addr];
	}
	void writeHandler(PhysPt start, Bit8u val) {
		/* FIXME: "Chained EGA" how does that work?? */
		ModeOperation(val);
		/* Update video memory and the pixel buffer */
		vga.mem.linear[start] = val;
	}
public:	
	VGA_ChainedEGA_Handler() : PageHandler(PFLAG_NOCODE) {}
	void writeb(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED(addr);
		writeHandler(addr+0,(Bit8u)(val >> 0));
	}
	void writew(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED(addr);
		writeHandler(addr+0,(Bit8u)(val >> 0));
		writeHandler(addr+1,(Bit8u)(val >> 8));
	}
	void writed(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED(addr);
		writeHandler(addr+0,(Bit8u)(val >> 0));
		writeHandler(addr+1,(Bit8u)(val >> 8));
		writeHandler(addr+2,(Bit8u)(val >> 16));
		writeHandler(addr+3,(Bit8u)(val >> 24));
	}
	Bitu readb(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED(addr);
		return readHandler(addr);
	}
	Bitu readw(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED(addr);
		Bitu ret = (readHandler(addr+0) << 0);
		ret     |= (readHandler(addr+1) << 8);
		return ret;
	}
	Bitu readd(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED(addr);
		Bitu ret = (readHandler(addr+0) << 0);
		ret     |= (readHandler(addr+1) << 8);
		ret     |= (readHandler(addr+2) << 16);
		ret     |= (readHandler(addr+3) << 24);
		return ret;
	}
};

class VGA_UnchainedEGA_Handler : public VGA_UnchainedRead_Handler {
public:
	VGA_UnchainedEGA_Handler(Bitu flags) : VGA_UnchainedRead_Handler(flags) {}
	template< bool wrapping>
	void writeHandler(PhysPt start, Bit8u val) {
		Bit32u data=ModeOperation(val);
		/* Update video memory and the pixel buffer */
		VGA_Latch pixels;
		pixels.d=((Bit32u*)vga.mem.linear)[start];
		pixels.d&=vga.config.full_not_map_mask;
		pixels.d|=(data & vga.config.full_map_mask);
		((Bit32u*)vga.mem.linear)[start]=pixels.d;
	}
public:	
	VGA_UnchainedEGA_Handler() : VGA_UnchainedRead_Handler(PFLAG_NOCODE) {}
	void writeb(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED2(addr);
		writeHandler<true>(addr+0,(Bit8u)(val >> 0));
	}
	void writew(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED2(addr);
		writeHandler<true>(addr+0,(Bit8u)(val >> 0));
		writeHandler<true>(addr+1,(Bit8u)(val >> 8));
	}
	void writed(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED2(addr);
		writeHandler<true>(addr+0,(Bit8u)(val >> 0));
		writeHandler<true>(addr+1,(Bit8u)(val >> 8));
		writeHandler<true>(addr+2,(Bit8u)(val >> 16));
		writeHandler<true>(addr+3,(Bit8u)(val >> 24));
	}
};

// Slow accurate emulation.
// This version takes the Graphics Controller bitmask and ROPs into account.
// This is needed for demos that use the bitmask to do color combination or bitplane "page flipping" tricks.
// This code will kick in if running in a chained VGA mode and the graphics controller bitmask register is
// changed to anything other than 0xFF.
//
// Impact Studios "Legend"
//  - The rotating objects, rendered as dots, needs this hack because it uses a combination of masking off
//    bitplanes using the VGA DAC pel mask and drawing on the hidden bitplane using the Graphics Controller
//    bitmask. It also relies on loading the VGA latches with zeros as a form of "overdraw". Without this
//    version the effect will instead become a glowing ball of flickering yellow/red.
class VGA_ChainedVGA_Slow_Handler : public PageHandler {
public:
	VGA_ChainedVGA_Slow_Handler() : PageHandler(PFLAG_NOCODE) {}
	static INLINE Bitu readHandler8(PhysPt addr ) {
		vga.latch.d=((Bit32u*)vga.mem.linear)[addr&~3];
		return vga.latch.b[addr&3];
	}
	static INLINE void writeHandler8(PhysPt addr, Bitu val) {
		VGA_Latch pixels;

		/* byte-sized template specialization with masking */
		pixels.d = ModeOperation(val);
		/* Update video memory and the pixel buffer */
		hostWrite<Bit8u>( &vga.mem.linear[((addr&~3)<<2)+(addr&3)], pixels.b[addr&3] );
	}
	Bitu readb(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED(addr);
		return readHandler8( addr );
	}
	Bitu readw(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED(addr);
		Bitu ret = (readHandler8( addr+0 ) << 0 );
		ret     |= (readHandler8( addr+1 ) << 8 );
		return ret;
	}
	Bitu readd(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED(addr);
		Bitu ret = (readHandler8( addr+0 ) << 0 );
		ret     |= (readHandler8( addr+1 ) << 8 );
		ret     |= (readHandler8( addr+2 ) << 16 );
		ret     |= (readHandler8( addr+3 ) << 24 );
		return ret;
	}
	void writeb(PhysPt addr, Bitu val ) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED(addr);
		writeHandler8( addr, val );
	}
	void writew(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED(addr);
		writeHandler8( addr+0, val >> 0 );
		writeHandler8( addr+1, val >> 8 );
	}
	void writed(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED(addr);
		writeHandler8( addr+0, val >> 0 );
		writeHandler8( addr+1, val >> 8 );
		writeHandler8( addr+2, val >> 16 );
		writeHandler8( addr+3, val >> 24 );
	}
};

//Slighly unusual version, will directly write 8,16,32 bits values
class VGA_ChainedVGA_Handler : public PageHandler {
public:
	VGA_ChainedVGA_Handler() : PageHandler(PFLAG_NOCODE) {}
	template <class Size>
	static INLINE Bitu readHandler(PhysPt addr ) {
		return hostRead<Size>( &vga.mem.linear[((addr&0xFFFC)<<2)+(addr&3)] );
	}
	template <class Size>
	static INLINE void writeHandler(PhysPt addr, Bitu val) {
		// No need to check for compatible chains here, this one is only enabled if that bit is set
		hostWrite<Size>( &vga.mem.linear[((addr&0xFFFC)<<2)+(addr&3)], val );
	}
	Bitu readb(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED(addr);
		return readHandler<Bit8u>( addr );
	}
	Bitu readw(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED(addr);
		if (GCC_UNLIKELY(addr & 1)) {
			Bitu ret = (readHandler<Bit8u>( addr+0 ) << 0 );
			ret     |= (readHandler<Bit8u>( addr+1 ) << 8 );
			return ret;
		} else
			return readHandler<Bit16u>( addr );
	}
	Bitu readd(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED(addr);
		if (GCC_UNLIKELY(addr & 3)) {
			Bitu ret = (readHandler<Bit8u>( addr+0 ) << 0 );
			ret     |= (readHandler<Bit8u>( addr+1 ) << 8 );
			ret     |= (readHandler<Bit8u>( addr+2 ) << 16 );
			ret     |= (readHandler<Bit8u>( addr+3 ) << 24 );
			return ret;
		} else
			return readHandler<Bit32u>( addr );
	}
	void writeb(PhysPt addr, Bitu val ) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED(addr);
		writeHandler<Bit8u>( addr, val );
	}
	void writew(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED(addr);
		if (GCC_UNLIKELY(addr & 1)) {
			writeHandler<Bit8u>( addr+0, val >> 0 );
			writeHandler<Bit8u>( addr+1, val >> 8 );
		} else {
			writeHandler<Bit16u>( addr, val );
		}
	}
	void writed(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED(addr);
		if (GCC_UNLIKELY(addr & 3)) {
			writeHandler<Bit8u>( addr+0, val >> 0 );
			writeHandler<Bit8u>( addr+1, val >> 8 );
			writeHandler<Bit8u>( addr+2, val >> 16 );
			writeHandler<Bit8u>( addr+3, val >> 24 );
		} else {
			writeHandler<Bit32u>( addr, val );
		}
	}
};

// alternate version for ET4000 emulation.
// ET4000 cards implement 256-color chain-4 differently than most cards.
class VGA_ET4000_ChainedVGA_Handler : public PageHandler {
public:
	VGA_ET4000_ChainedVGA_Handler() : PageHandler(PFLAG_NOCODE) {}
	template <class Size>
	static INLINE Bitu readHandler(PhysPt addr ) {
		return hostRead<Size>( &vga.mem.linear[addr] );
	}
	template <class Size>
	static INLINE void writeHandler(PhysPt addr, Bitu val) {
		// No need to check for compatible chains here, this one is only enabled if that bit is set
		hostWrite<Size>( &vga.mem.linear[addr], val );
	}
	Bitu readb(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED(addr);
		return readHandler<Bit8u>( addr );
	}
	Bitu readw(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED(addr);
		if (GCC_UNLIKELY(addr & 1)) {
			Bitu ret = (readHandler<Bit8u>( addr+0 ) << 0 );
			ret     |= (readHandler<Bit8u>( addr+1 ) << 8 );
			return ret;
		} else
			return readHandler<Bit16u>( addr );
	}
	Bitu readd(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED(addr);
		if (GCC_UNLIKELY(addr & 3)) {
			Bitu ret = (readHandler<Bit8u>( addr+0 ) << 0 );
			ret     |= (readHandler<Bit8u>( addr+1 ) << 8 );
			ret     |= (readHandler<Bit8u>( addr+2 ) << 16 );
			ret     |= (readHandler<Bit8u>( addr+3 ) << 24 );
			return ret;
		} else
			return readHandler<Bit32u>( addr );
	}
	void writeb(PhysPt addr, Bitu val ) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED(addr);
		writeHandler<Bit8u>( addr, val );
	}
	void writew(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED(addr);
		if (GCC_UNLIKELY(addr & 1)) {
			writeHandler<Bit8u>( addr+0, val >> 0 );
			writeHandler<Bit8u>( addr+1, val >> 8 );
		} else {
			writeHandler<Bit16u>( addr, val );
		}
	}
	void writed(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED(addr);
		if (GCC_UNLIKELY(addr & 3)) {
			writeHandler<Bit8u>( addr+0, val >> 0 );
			writeHandler<Bit8u>( addr+1, val >> 8 );
			writeHandler<Bit8u>( addr+2, val >> 16 );
			writeHandler<Bit8u>( addr+3, val >> 24 );
		} else {
			writeHandler<Bit32u>( addr, val );
		}
	}
};

class VGA_ET4000_ChainedVGA_Slow_Handler : public PageHandler {
public:
	VGA_ET4000_ChainedVGA_Slow_Handler() : PageHandler(PFLAG_NOCODE) {}
	static INLINE Bitu readHandler8(PhysPt addr ) {
		vga.latch.d=((Bit32u*)vga.mem.linear)[addr>>2];
		return vga.latch.b[addr&3];
	}
	static INLINE void writeHandler8(PhysPt addr, Bitu val) {
		VGA_Latch pixels;

		/* byte-sized template specialization with masking */
		pixels.d = ModeOperation(val);
		/* Update video memory and the pixel buffer */
		hostWrite<Bit8u>( &vga.mem.linear[addr], pixels.b[addr&3] );
	}
	Bitu readb(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED(addr);
		return readHandler8( addr );
	}
	Bitu readw(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED(addr);
		Bitu ret = (readHandler8( addr+0 ) << 0 );
		ret     |= (readHandler8( addr+1 ) << 8 );
		return ret;
	}
	Bitu readd(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_read_full;
		addr = CHECKED(addr);
		Bitu ret = (readHandler8( addr+0 ) << 0 );
		ret     |= (readHandler8( addr+1 ) << 8 );
		ret     |= (readHandler8( addr+2 ) << 16 );
		ret     |= (readHandler8( addr+3 ) << 24 );
		return ret;
	}
	void writeb(PhysPt addr, Bitu val ) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED(addr);
		writeHandler8( addr, val );
	}
	void writew(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED(addr);
		writeHandler8( addr+0, val >> 0 );
		writeHandler8( addr+1, val >> 8 );
	}
	void writed(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED(addr);
		writeHandler8( addr+0, val >> 0 );
		writeHandler8( addr+1, val >> 8 );
		writeHandler8( addr+2, val >> 16 );
		writeHandler8( addr+3, val >> 24 );
	}
};

class VGA_UnchainedVGA_Handler : public VGA_UnchainedRead_Handler {
public:
	void writeHandler( PhysPt addr, Bit8u val ) {
		PhysPt memaddr = addr;
		Bit32u data=ModeOperation(val);
		VGA_Latch pixels;

		if (vga.gfx.miscellaneous&2) /* Odd/Even mode masks off A0 */
			memaddr &= ~1;

		pixels.d=((Bit32u*)vga.mem.linear)[memaddr];

		/* Odd/even emulation, emulation fix for Windows 95's boot screen */
		if (!(vga.seq.memory_mode&4)) {
			/* You're probably wondering what the hell odd/even mode has to do with Windows 95's boot
			 * screen, right? Well, hopefully you won't puke when you read the following...
			 * 
			 * When Windows 95 starts up and shows it's boot logo, it calls INT 10h to set mode 0x13.
			 * But it calls INT 10h with AX=0x93 which means set mode 0x13 and don't clear VRAM. Then,
			 * it uses mode X to write the logo to the BOTTOM half of VGA RAM, at 0x8000 to be exact,
			 * and of course, reprograms the CRTC offset register to make that visible.
			 * THEN, it reprograms the registers to map VRAM at 0xB800, disable Chain 4, re-enable
			 * odd/even mode, and then allows both DOS and the BIOS to write to the top half of VRAM
			 * as if still running in 80x25 alphanumeric text mode. It even sets the video mode byte
			 * at 0x40:0x49 to 0x03 to continue the illusion!
			 *
			 * When Windows 95 is ready to restore text mode, it just switches back (this time, calling
			 * the saved INT 10h pointer directly) again without clearing VRAM.
			 *
			 * So if you wonder why I would spend time implementing odd/even emulation for VGA unchained
			 * mode... that's why. You can thank Microsoft for that. */
			if (addr & 1) {
				if (vga.seq.map_mask & 0x2) /* bitplane 1: attribute RAM */
					pixels.b[1] = data >> 8;
				if (vga.seq.map_mask & 0x8) /* bitplane 3: unused RAM */
					pixels.b[3] = data >> 24;
			}
			else {
				if (vga.seq.map_mask & 0x1) /* bitplane 0: character RAM */
					pixels.b[0] = data;
				if (vga.seq.map_mask & 0x4) { /* bitplane 2: font RAM */
					pixels.b[2] = data >> 16;
					vga.draw.font[memaddr] = data >> 16;
				}
			}
		}
		else {
			pixels.d&=vga.config.full_not_map_mask;
			pixels.d|=(data & vga.config.full_map_mask);
		}

		((Bit32u*)vga.mem.linear)[memaddr]=pixels.d;
	}
public:
	VGA_UnchainedVGA_Handler() : VGA_UnchainedRead_Handler(PFLAG_NOCODE) {}
	void writeb(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED2(addr);
		writeHandler(addr+0,(Bit8u)(val >> 0));
	}
	void writew(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED2(addr);
		writeHandler(addr+0,(Bit8u)(val >> 0));
		writeHandler(addr+1,(Bit8u)(val >> 8));
	}
	void writed(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += vga.svga.bank_write_full;
		addr = CHECKED2(addr);
		writeHandler(addr+0,(Bit8u)(val >> 0));
		writeHandler(addr+1,(Bit8u)(val >> 8));
		writeHandler(addr+2,(Bit8u)(val >> 16));
		writeHandler(addr+3,(Bit8u)(val >> 24));
	}
};

#include <stdio.h>

class VGA_CGATEXT_PageHandler : public PageHandler {
public:
	VGA_CGATEXT_PageHandler() {
		flags=PFLAG_NOCODE;
	}
	Bitu readb(PhysPt addr) {
		addr = PAGING_GetPhysicalAddress(addr) & 0x3FFF;
		VGAMEM_USEC_read_delay();
		return vga.tandy.mem_base[addr];
	}
	void writeb(PhysPt addr,Bitu val){
		VGAMEM_USEC_write_delay();

		if (enableCGASnow) {
			/* NTS: We can't use PIC_FullIndex() exclusively because it's not precise enough
			 *      with respect to when DOSBox CPU emulation is writing. We have to use other
			 *      variables like CPU_Cycles to gain additional precision */
			double timeInFrame = PIC_FullIndex()-vga.draw.delay.framestart;
			double timeInLine = fmod(timeInFrame,vga.draw.delay.htotal);

			/* we're in active area. which column should the snow show up on? */
			Bit32u x = (Bit32u)((timeInLine * 80) / vga.draw.delay.hblkstart);
			if ((unsigned)x < 80) vga.draw.cga_snow[x] = val;
		}

		addr = PAGING_GetPhysicalAddress(addr) & 0x3FFF;
		vga.tandy.mem_base[addr] = val;
	}
};

/* The NEC display is documented to have:
 *
 * A0000-A3FFF      T-RAM (text) (8KB WORDs)
 *   A0000-A1FFF      Characters (4KB WORDs)
 *   A2000-A3FFF      Attributes (4KB WORDs). For each 16-bit WORD only the lower 8 bits are read/writeable.
 *   A4000-A5FFF      Unknown ?? (4KB WORDs)
 *   A6000-A7FFF      Not present (4KB WORDs)
 * A8000-BFFFF      G-RAM (graphics) (96KB)
 *
 * T-RAM character display RAM is 16-bits per character.
 * ASCII text has upper 8 bits zero.
 * SHIFT-JIS doublewide characters use the upper byte for non-ASCII. */

class VGA_PC98_PageHandler : public PageHandler {
public:
	VGA_PC98_PageHandler() : PageHandler(PFLAG_NOCODE) {}
	Bitu readb(PhysPt addr) {
		VGAMEM_USEC_read_delay(); // FIXME: VRAM delay? How fast is the VRAM compared to the CPU?

		addr = PAGING_GetPhysicalAddress(addr);

        /* FIXME: What exactly is at E0000-E7FFF? */
        if (addr >= 0xE0000) return ~0;

        addr &= 0x1FFFF;
        switch (addr>>13) {
            case 0:     /* A0000-A1FFF Character RAM */
                break;
            case 1:     /* A2000-A3FFF Attribute RAM */
                if (addr & 1) return ~0; /* ignore odd bytes */
                break;
            case 2:     /* A4000-A5FFF Unknown ?? */
                break;
            case 3:     /* A6000-A7FFF Not present */
                return ~0;
            default:    /* A8000-BFFFF G-RAM */
                break;
        };

		return vga.mem.linear[addr];
	}
	void writeb(PhysPt addr,Bitu val){
		VGAMEM_USEC_read_delay(); // FIXME: VRAM delay? How fast is the VRAM compared to the CPU?

		addr = PAGING_GetPhysicalAddress(addr);

        /* FIXME: What exactly is at E0000-E7FFF? */
        if (addr >= 0xE0000) return;

        addr &= 0x1FFFF;
        switch (addr>>13) {
            case 0:     /* A0000-A1FFF Character RAM */
                break;
            case 1:     /* A2000-A3FFF Attribute RAM */
                if (addr & 1) return; /* ignore odd bytes */
                break;
            case 2:     /* A4000-A5FFF Unknown ?? */
                break;
            case 3:     /* A6000-A7FFF Not present */
                return;
            default:    /* A8000-BFFFF G-RAM */
                break;
        };

		vga.mem.linear[addr] = val;
	}
};

class VGA_TEXT_PageHandler : public PageHandler {
public:
	VGA_TEXT_PageHandler() : PageHandler(PFLAG_NOCODE) {}
	Bitu readb(PhysPt addr) {
		unsigned char bplane;

		VGAMEM_USEC_read_delay();

		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		bplane = vga.gfx.read_map_select;

		if (!(vga.seq.memory_mode&4))
			bplane = (bplane & ~1) + (addr & 1); /* FIXME: Is this what VGA cards do? It makes sense to me */
		if (vga.gfx.miscellaneous&2) /* Odd/Even mode */
			addr &= ~1;

		return vga.mem.linear[CHECKED3(vga.svga.bank_read_full+(addr<<2)+bplane)];
	}
	void writeb(PhysPt addr,Bitu val){
		VGA_Latch pixels;
		Bitu memaddr;

		VGAMEM_USEC_write_delay();

		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		memaddr = addr;

		/* Chain Odd/Even enable: A0 is replaced by a "higher order bit" (0 apparently) */
		if (vga.gfx.miscellaneous&2)
			memaddr &= ~1;

		pixels.d=((Bit32u*)vga.mem.linear)[memaddr];

		if ((vga.seq.memory_mode&4)/*Odd/Even disable*/ || (addr & 1)) {
			if (vga.seq.map_mask & 0x2) /* bitplane 1: attribute RAM */
				pixels.b[1] = val;
			if (vga.seq.map_mask & 0x8) /* bitplane 3: unused RAM */
				pixels.b[3] = val;
		}
		if ((vga.seq.memory_mode&4)/*Odd/Even disable*/ || !(addr & 1)) {
			if (vga.seq.map_mask & 0x1) /* bitplane 0: character RAM */
				pixels.b[0] = val;
			if (vga.seq.map_mask & 0x4) { /* bitplane 2: font RAM */
				pixels.b[2] = val;
				vga.draw.font[memaddr] = val;
			}
		}

		((Bit32u*)vga.mem.linear)[memaddr]=pixels.d;
	}
};

class VGA_Map_Handler : public PageHandler {
public:
	VGA_Map_Handler() : PageHandler(PFLAG_READABLE|PFLAG_WRITEABLE|PFLAG_NOCODE) {}
	HostPt GetHostReadPt(Bitu phys_page) {
 		phys_page-=vgapages.base;
		return &vga.mem.linear[CHECKED3(vga.svga.bank_read_full+phys_page*4096)];
	}
	HostPt GetHostWritePt(Bitu phys_page) {
 		phys_page-=vgapages.base;
		return &vga.mem.linear[CHECKED3(vga.svga.bank_write_full+phys_page*4096)];
	}
};

class VGA_Slow_CGA_Handler : public PageHandler {
public:
	VGA_Slow_CGA_Handler() : PageHandler(PFLAG_NOCODE) {}
	void delay() {
		Bits delaycyc = CPU_CycleMax/((Bit32u)(1024/2.80)); 
		if(GCC_UNLIKELY(CPU_Cycles < 3*delaycyc)) delaycyc=0;
		CPU_Cycles -= delaycyc;
		CPU_IODelayRemoved += delaycyc;
	}

	Bitu readb(PhysPt addr) {
		delay();
		return vga.tandy.mem_base[addr - 0xb8000];
	}
	void writeb(PhysPt addr,Bitu val){
		delay();
		vga.tandy.mem_base[addr - 0xb8000] = (Bit8u) val;
	}
	
};

class VGA_LIN4_Handler : public VGA_UnchainedEGA_Handler {
public:
	VGA_LIN4_Handler() : VGA_UnchainedEGA_Handler(PFLAG_NOCODE) {}
	void writeb(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = vga.svga.bank_write_full + (PAGING_GetPhysicalAddress(addr) & 0xffff);
		addr = CHECKED4(addr);
		writeHandler<false>(addr+0,(Bit8u)(val >> 0));
	}
	void writew(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = vga.svga.bank_write_full + (PAGING_GetPhysicalAddress(addr) & 0xffff);
		addr = CHECKED4(addr);
		writeHandler<false>(addr+0,(Bit8u)(val >> 0));
		writeHandler<false>(addr+1,(Bit8u)(val >> 8));
	}
	void writed(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = vga.svga.bank_write_full + (PAGING_GetPhysicalAddress(addr) & 0xffff);
		addr = CHECKED4(addr);
		writeHandler<false>(addr+0,(Bit8u)(val >> 0));
		writeHandler<false>(addr+1,(Bit8u)(val >> 8));
		writeHandler<false>(addr+2,(Bit8u)(val >> 16));
		writeHandler<false>(addr+3,(Bit8u)(val >> 24));
	}
	Bitu readb(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = vga.svga.bank_read_full + (PAGING_GetPhysicalAddress(addr) & 0xffff);
		addr = CHECKED4(addr);
		return readHandler(addr);
	}
	Bitu readw(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = vga.svga.bank_read_full + (PAGING_GetPhysicalAddress(addr) & 0xffff);
		addr = CHECKED4(addr);
		Bitu ret = (readHandler(addr+0) << 0);
		ret     |= (readHandler(addr+1) << 8);
		return ret;
	}
	Bitu readd(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = vga.svga.bank_read_full + (PAGING_GetPhysicalAddress(addr) & 0xffff);
		addr = CHECKED4(addr);
		Bitu ret = (readHandler(addr+0) << 0);
		ret     |= (readHandler(addr+1) << 8);
		ret     |= (readHandler(addr+2) << 16);
		ret     |= (readHandler(addr+3) << 24);
		return ret;
	}
};

class VGA_LFB_Handler : public PageHandler {
public:
	VGA_LFB_Handler() : PageHandler(PFLAG_READABLE|PFLAG_WRITEABLE|PFLAG_NOCODE) {}
	HostPt GetHostReadPt( Bitu phys_page ) {
		phys_page -= vga.lfb.page;
		phys_page &= (vga.vmemsize >> 12) - 1;
		return &vga.mem.linear[CHECKED3(phys_page * 4096)];
	}
	HostPt GetHostWritePt( Bitu phys_page ) {
		return GetHostReadPt( phys_page );
	}
};

extern void XGA_Write(Bitu port, Bitu val, Bitu len);
extern Bitu XGA_Read(Bitu port, Bitu len);

class VGA_MMIO_Handler : public PageHandler {
public:
	VGA_MMIO_Handler() : PageHandler(PFLAG_NOCODE) {}
	void writeb(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		XGA_Write(port, val, 1);
	}
	void writew(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		XGA_Write(port, val, 2);
	}
	void writed(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		XGA_Write(port, val, 4);
	}

	Bitu readb(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		return XGA_Read(port, 1);
	}
	Bitu readw(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		return XGA_Read(port, 2);
	}
	Bitu readd(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		return XGA_Read(port, 4);
	}
};

class VGA_TANDY_PageHandler : public PageHandler {
public:
	VGA_TANDY_PageHandler() : PageHandler(PFLAG_READABLE|PFLAG_WRITEABLE) {}
	HostPt GetHostReadPt(Bitu phys_page) {
		// Odd banks are limited to 16kB and repeated
		if (vga.tandy.mem_bank & 1) 
			phys_page&=0x03;
		else 
			phys_page&=0x07;
		return vga.tandy.mem_base + (phys_page * 4096);
	}
	HostPt GetHostWritePt(Bitu phys_page) {
		return GetHostReadPt( phys_page );
	}
};


class VGA_PCJR_Handler : public PageHandler {
public:
	VGA_PCJR_Handler() : PageHandler(PFLAG_READABLE|PFLAG_WRITEABLE) {}
	HostPt GetHostReadPt(Bitu phys_page) {
		phys_page-=0xb8;
		// The 16kB map area is repeated in the 32kB range
		// On CGA CPU A14 is not decoded so it repeats there too
		phys_page&=0x03;
		return vga.tandy.mem_base + (phys_page * 4096);
	}
	HostPt GetHostWritePt(Bitu phys_page) {
		return GetHostReadPt( phys_page );
	}
};

class VGA_AMS_Handler : public PageHandler {
public:
	template< bool wrapping>
	void writeHandler(PhysPt start, Bit8u val) {
		vga.tandy.mem_base[ start ] = val;
#ifdef DIJDIJD
		Bit32u data=ModeOperation(val);
		/* Update video memory and the pixel buffer */
		VGA_Latch pixels;
		pixels.d=((Bit32u*)vga.mem.linear)[start];
		pixels.d&=vga.config.full_not_map_mask;
		pixels.d|=(data & vga.config.full_map_mask);
		((Bit32u*)vga.mem.linear)[start]=pixels.d;
		Bit8u * write_pixels=&vga.mem.linear[VGA_CACHE_OFFSET+(start<<3)];

		Bit32u colors0_3, colors4_7;
		VGA_Latch temp;temp.d=(pixels.d>>4) & 0x0f0f0f0f;
			colors0_3 = 
			Expand16Table[0][temp.b[0]] |
			Expand16Table[1][temp.b[1]] |
			Expand16Table[2][temp.b[2]] |
			Expand16Table[3][temp.b[3]];
		*(Bit32u *)write_pixels=colors0_3;
		temp.d=pixels.d & 0x0f0f0f0f;
		colors4_7 = 
			Expand16Table[0][temp.b[0]] |
			Expand16Table[1][temp.b[1]] |
			Expand16Table[2][temp.b[2]] |
			Expand16Table[3][temp.b[3]];
		*(Bit32u *)(write_pixels+4)=colors4_7;
		if (wrapping && GCC_UNLIKELY( start < 512)) {
			*(Bit32u *)(write_pixels+512*1024)=colors0_3;
			*(Bit32u *)(write_pixels+512*1024+4)=colors4_7;
		}
#endif
	}
//	template< bool wrapping>
	Bit8u readHandler(PhysPt start) {
		return vga.tandy.mem_base[ start ];
	}

public:
	VGA_AMS_Handler() {
		//flags=PFLAG_READABLE|PFLAG_WRITEABLE;
		flags=PFLAG_NOCODE;
	}
	inline PhysPt wrAddr( PhysPt addr )
	{
		if( vga.mode != M_AMSTRAD )
		{
			addr -= 0xb8000;
			Bitu phys_page = addr >> 12;
			//test for a unaliged bank, then replicate 2x16kb
			if (vga.tandy.mem_bank & 1) 
				phys_page&=0x03;
			return ( phys_page * 4096 ) + ( addr & 0x0FFF );
		}
		return ( (PAGING_GetPhysicalAddress(addr) & 0xffff) - 0x8000 ) & ( 32*1024-1 );
	}

	void writeb(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = wrAddr( addr );
		Bitu plane = vga.mode==M_AMSTRAD ? vga.amstrad.write_plane : 0x01; // 0x0F?
		if( plane & 0x08 ) writeHandler<false>(addr+49152,(Bit8u)(val >> 0));
		if( plane & 0x04 ) writeHandler<false>(addr+32768,(Bit8u)(val >> 0));
		if( plane & 0x02 ) writeHandler<false>(addr+16384,(Bit8u)(val >> 0));
		if( plane & 0x01 ) writeHandler<false>(addr+0,(Bit8u)(val >> 0));
	}
	void writew(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = wrAddr( addr );
		Bitu plane = vga.mode==M_AMSTRAD ? vga.amstrad.write_plane : 0x01; // 0x0F?
		if( plane & 0x01 )
		{
			writeHandler<false>(addr+0,(Bit8u)(val >> 0));
			writeHandler<false>(addr+1,(Bit8u)(val >> 8));
		}
		addr += 16384;
		if( plane & 0x02 )
		{
			writeHandler<false>(addr+0,(Bit8u)(val >> 0));
			writeHandler<false>(addr+1,(Bit8u)(val >> 8));
		}
		addr += 16384;
		if( plane & 0x04 )
		{
			writeHandler<false>(addr+0,(Bit8u)(val >> 0));
			writeHandler<false>(addr+1,(Bit8u)(val >> 8));
		}
		addr += 16384;
		if( plane & 0x08 )
		{
			writeHandler<false>(addr+0,(Bit8u)(val >> 0));
			writeHandler<false>(addr+1,(Bit8u)(val >> 8));
		}

	}
	void writed(PhysPt addr,Bitu val) {
		VGAMEM_USEC_write_delay();
		addr = wrAddr( addr );
		Bitu plane = vga.mode==M_AMSTRAD ? vga.amstrad.write_plane : 0x01; // 0x0F?
		if( plane & 0x01 )
		{
			writeHandler<false>(addr+0,(Bit8u)(val >> 0));
			writeHandler<false>(addr+1,(Bit8u)(val >> 8));
			writeHandler<false>(addr+2,(Bit8u)(val >> 16));
			writeHandler<false>(addr+3,(Bit8u)(val >> 24));
		}
		addr += 16384;
		if( plane & 0x02 )
		{
			writeHandler<false>(addr+0,(Bit8u)(val >> 0));
			writeHandler<false>(addr+1,(Bit8u)(val >> 8));
			writeHandler<false>(addr+2,(Bit8u)(val >> 16));
			writeHandler<false>(addr+3,(Bit8u)(val >> 24));
		}
		addr += 16384;
		if( plane & 0x04 )
		{
			writeHandler<false>(addr+0,(Bit8u)(val >> 0));
			writeHandler<false>(addr+1,(Bit8u)(val >> 8));
			writeHandler<false>(addr+2,(Bit8u)(val >> 16));
			writeHandler<false>(addr+3,(Bit8u)(val >> 24));
		}
		addr += 16384;
		if( plane & 0x08 )
		{
			writeHandler<false>(addr+0,(Bit8u)(val >> 0));
			writeHandler<false>(addr+1,(Bit8u)(val >> 8));
			writeHandler<false>(addr+2,(Bit8u)(val >> 16));
			writeHandler<false>(addr+3,(Bit8u)(val >> 24));
		}

	}
	Bitu readb(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = wrAddr( addr ) + ( vga.amstrad.read_plane * 16384 );
		addr &= (64*1024-1);
		return readHandler(addr);
	}
	Bitu readw(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = wrAddr( addr ) + ( vga.amstrad.read_plane * 16384 );
		addr &= (64*1024-1);
		return 
			(readHandler(addr+0) << 0) |
			(readHandler(addr+1) << 8);
	}
	Bitu readd(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = wrAddr( addr ) + ( vga.amstrad.read_plane * 16384 );
		addr &= (64*1024-1);
		return 
			(readHandler(addr+0) << 0)  |
			(readHandler(addr+1) << 8)  |
			(readHandler(addr+2) << 16) |
			(readHandler(addr+3) << 24);
	}

/*
	HostPt GetHostReadPt(Bitu phys_page)
	{
		if( vga.mode!=M_AMSTRAD )
		{
			phys_page-=0xb8;
			//test for a unaliged bank, then replicate 2x16kb
			if (vga.tandy.mem_bank & 1) 
				phys_page&=0x03;
			return vga.tandy.mem_base + (phys_page * 4096);
		}
		phys_page-=0xb8;
		return vga.tandy.mem_base + (phys_page*4096) + (vga.amstrad.read_plane * 16384) ;
	}
*/
/*
	HostPt GetHostWritePt(Bitu phys_page) {
		return GetHostReadPt( phys_page );
	}
*/
};

class VGA_HERC_Handler : public PageHandler {
public:
	VGA_HERC_Handler() {
		flags=PFLAG_READABLE|PFLAG_WRITEABLE;
	}
	HostPt GetHostReadPt(Bitu phys_page) {
		// The 4kB map area is repeated in the 32kB range
		return &vga.mem.linear[0];
	}
	HostPt GetHostWritePt(Bitu phys_page) {
		return GetHostReadPt( phys_page );
	}
};

class VGA_Empty_Handler : public PageHandler {
public:
	VGA_Empty_Handler() : PageHandler(PFLAG_NOCODE) {}
	Bitu readb(PhysPt /*addr*/) {
//		LOG(LOG_VGA, LOG_NORMAL ) ( "Read from empty memory space at %x", addr );
		return 0xff;
	} 
	void writeb(PhysPt /*addr*/,Bitu /*val*/) {
//		LOG(LOG_VGA, LOG_NORMAL ) ( "Write %x to empty memory space at %x", val, addr );
	}
};

static struct vg {
	VGA_Map_Handler				map;
	VGA_Slow_CGA_Handler		slow;
	VGA_TEXT_PageHandler		text;
	VGA_CGATEXT_PageHandler		cgatext;
	VGA_TANDY_PageHandler		tandy;
	VGA_ChainedEGA_Handler		cega;
	VGA_ChainedVGA_Handler		cvga;
	VGA_ChainedVGA_Slow_Handler	cvga_slow;
	VGA_ET4000_ChainedVGA_Handler		cvga_et4000;
	VGA_ET4000_ChainedVGA_Slow_Handler	cvga_et4000_slow;
	VGA_UnchainedEGA_Handler	uega;
	VGA_UnchainedVGA_Handler	uvga;
	VGA_PCJR_Handler			pcjr;
	VGA_HERC_Handler			herc;
	VGA_LIN4_Handler			lin4;
	VGA_LFB_Handler				lfb;
	VGA_MMIO_Handler			mmio;
	VGA_AMS_Handler				ams;
    VGA_PC98_PageHandler        pc98;
	VGA_Empty_Handler			empty;
} vgaph;

void VGA_ChangedBank(void) {
	VGA_SetupHandlers();
}

void MEM_ResetPageHandler_Unmapped(Bitu phys_page, Bitu pages);
void MEM_ResetPageHandler_RAM(Bitu phys_page, Bitu pages);

extern bool adapter_rom_is_ram;

void VGA_SetupHandlers(void) {
	vga.svga.bank_read_full = vga.svga.bank_read*vga.svga.bank_size;
	vga.svga.bank_write_full = vga.svga.bank_write*vga.svga.bank_size;

	PageHandler *newHandler;
	switch (machine) {
	case MCH_CGA:
		if (enableCGASnow && (vga.mode == M_TEXT || vga.mode == M_TANDY_TEXT))
			MEM_SetPageHandler( VGA_PAGE_B8, 8, &vgaph.cgatext );
		else
			MEM_SetPageHandler( VGA_PAGE_B8, 8, &vgaph.slow );
		goto range_done;
	case MCH_PCJR:
		MEM_SetPageHandler( VGA_PAGE_B8, 8, &vgaph.pcjr );
		goto range_done;
	case MCH_HERC:
		vgapages.base=VGA_PAGE_B0;
		/* NTS: Implemented according to [http://www.seasip.info/VintagePC/hercplus.html#regs] */
		if (vga.herc.enable_bits & 0x2) { /* bit 1: page in upper 32KB */
			vgapages.mask=0xffff;
			/* NTS: I don't know what Hercules graphics cards do if you set bit 1 but not bit 0.
			 *      For the time being, I'm assuming that they respond to 0xB8000+ because of bit 1
			 *      but only map to the first 4KB because of bit 0. Basically, a configuration no
			 *      software would actually use. */
			if (vga.herc.enable_bits & 0x1) /* allow graphics and enable 0xB1000-0xB7FFF */
				MEM_SetPageHandler(VGA_PAGE_B0,16,&vgaph.map);
			else
				MEM_SetPageHandler(VGA_PAGE_B0,16,&vgaph.herc);
		} else {
			vgapages.mask=0x7fff;
			// With hercules in 32kB mode it leaves a memory hole on 0xb800
			// and has MDA-compatible address wrapping when graphics are disabled
			if (vga.herc.enable_bits & 0x1)
				MEM_SetPageHandler(VGA_PAGE_B0,8,&vgaph.map);
			else
				MEM_SetPageHandler(VGA_PAGE_B0,8,&vgaph.herc);
			MEM_SetPageHandler(VGA_PAGE_B8,8,&vgaph.empty);
		}
		goto range_done;
	case MCH_TANDY:
		/* Always map 0xa000 - 0xbfff, might overwrite 0xb800 */
		vgapages.base=VGA_PAGE_A0;
		vgapages.mask=0x1ffff;
		MEM_SetPageHandler(VGA_PAGE_A0, 32, &vgaph.map );
		if ( vga.tandy.extended_ram & 1 ) {
			//You seem to be able to also map different 64kb banks, but have to figure that out
			//This seems to work so far though
			vga.tandy.draw_base = vga.mem.linear;
			vga.tandy.mem_base = vga.mem.linear;
		} else {
			vga.tandy.draw_base = TANDY_VIDBASE( vga.tandy.draw_bank * 16 * 1024);
			vga.tandy.mem_base = TANDY_VIDBASE( vga.tandy.mem_bank * 16 * 1024);
			MEM_SetPageHandler( 0xb8, 8, &vgaph.tandy );
		}
		goto range_done;
//		MEM_SetPageHandler(vga.tandy.mem_bank<<2,vga.tandy.is_32k_mode ? 0x08 : 0x04,range_handler);
	case MCH_AMSTRAD: // Memory handler.
		MEM_SetPageHandler( 0xb8, 8, &vgaph.ams );
		goto range_done;
	case EGAVGA_ARCH_CASE:
    case PC98_ARCH_CASE:
		break;
	default:
		LOG_MSG("Illegal machine type %d", machine );
		return;
	}

	/* This should be vga only */
	switch (vga.mode) {
	case M_ERROR:
	default:
		return;
	case M_LIN4:
		newHandler = &vgaph.lin4;
		break;	
	case M_LIN15:
	case M_LIN16:
	case M_LIN24:
	case M_LIN32:
		newHandler = &vgaph.map;
		break;
	case M_LIN8:
	case M_VGA:
		if (vga.config.chained) {
			bool slow = false;

			/* NTS: Most demos and games do not use the Graphics Controller ROPs or bitmask in chained
			 *      VGA modes. But, for the few that do, we have a "slow and accurate" implementation
			 *      that will handle these demos properly at the expense of some emulation speed.
			 *
			 *      This fixes:
			 *        Impact Studios 'Legend' demo (1993) */
			if (vga.config.full_bit_mask != 0xFFFFFFFF)
				slow = true;

			if (slow || vga.config.compatible_chain4) {
				/* NTS: ET4000AX cards appear to have a different chain4 implementation from everyone else:
				 *      the planar memory byte address is address >> 2 and bits A0-A1 select the plane,
				 *      where all other clones I've tested seem to write planar memory byte (address & ~3)
				 *      (one byte per 4 bytes) and bits A0-A1 select the plane. */
				/* FIXME: Different chain4 implementation on ET4000 noted---is it true also for ET3000? */
				if (svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K)
					newHandler = slow ? ((PageHandler*)(&vgaph.cvga_et4000_slow)) : ((PageHandler*)(&vgaph.cvga_et4000));
				else
					newHandler = slow ? ((PageHandler*)(&vgaph.cvga_slow)) : ((PageHandler*)(&vgaph.cvga));
			}
			else {
				newHandler = &vgaph.map;
			}
		} else {
			newHandler = &vgaph.uvga;
		}
		break;
	case M_EGA:
		if (vga.config.chained) 
			newHandler = &vgaph.cega;
		else
			newHandler = &vgaph.uega;
		break;	
	case M_TEXT:
	case M_CGA2:
	case M_CGA4:
		newHandler = &vgaph.text;
		break;
    case M_PC98:
		newHandler = &vgaph.pc98;

        /* We need something to catch access to E0000-E7FFF */
        MEM_SetPageHandler(0xE0, 8, newHandler );

        break;
	case M_AMSTRAD:
		newHandler = &vgaph.map;
		break;
	}
	switch ((vga.gfx.miscellaneous >> 2) & 3) {
	case 0:
		vgapages.base = VGA_PAGE_A0;
		switch (svgaCard) {
		case SVGA_TsengET3K:
			break;
		/* NTS: Looking at the official ET4000 programming guide, it does in fact support the full 128KB */
		case SVGA_S3Trio:
		default:
			vgapages.mask = 0x1ffff;
			break;
		}
		MEM_SetPageHandler(VGA_PAGE_A0, 32, newHandler );
		break;
	case 1:
		vgapages.base = VGA_PAGE_A0;
		vgapages.mask = 0xffff;
		MEM_SetPageHandler( VGA_PAGE_A0, 16, newHandler );
		if (adapter_rom_is_ram) MEM_ResetPageHandler_RAM( VGA_PAGE_B0, 16);
		else MEM_ResetPageHandler_Unmapped( VGA_PAGE_B0, 16);
		break;
	case 2:
		vgapages.base = VGA_PAGE_B0;
		vgapages.mask = 0x7fff;
		MEM_SetPageHandler( VGA_PAGE_B0, 8, newHandler );
		if (adapter_rom_is_ram) {
			MEM_ResetPageHandler_RAM( VGA_PAGE_A0, 16 );
			MEM_ResetPageHandler_RAM( VGA_PAGE_B8, 8 );
		}
		else {
			MEM_ResetPageHandler_Unmapped( VGA_PAGE_A0, 16 );
			MEM_ResetPageHandler_Unmapped( VGA_PAGE_B8, 8 );
		}
		break;
	case 3:
		vgapages.base = VGA_PAGE_B8;
		vgapages.mask = 0x7fff;
		MEM_SetPageHandler( VGA_PAGE_B8, 8, newHandler );
		if (adapter_rom_is_ram) {
			MEM_ResetPageHandler_RAM( VGA_PAGE_A0, 16 );
			MEM_ResetPageHandler_RAM( VGA_PAGE_B0, 8 );
		}
		else {
			MEM_ResetPageHandler_Unmapped( VGA_PAGE_A0, 16 );
			MEM_ResetPageHandler_Unmapped( VGA_PAGE_B0, 8 );
		}
		break;
	}
	if(svgaCard == SVGA_S3Trio && (vga.s3.ext_mem_ctrl & 0x10))
		MEM_SetPageHandler(VGA_PAGE_A0, 16, &vgaph.mmio);
range_done:
	PAGING_ClearTLB();
}

void VGA_StartUpdateLFB(void) {
	/* please obey the Linear Address Window Size register!
	 * Windows 3.1 S3 driver will reprogram the linear framebuffer down to 0xA0000 when entering a DOSBox
	 * and assuming the full VRAM size will cause a LOT of problems! */
	Bitu winsz = 0x10000;

	switch (vga.s3.reg_58&3) {
		case 1:
			winsz = 1 << 20;	//1MB
			break;
		case 2:
			winsz = 2 << 20;	//2MB
			break;
		case 3:
			winsz = 4 << 20;	//4MB
			break;
		// FIXME: What about the 8MB window?
	}

	/* if the DOS application or Windows 3.1 driver attempts to put the linear framebuffer
	 * below the top of memory, then we're probably entering a DOS VM and it's probably
	 * a 64KB window. If it's not a 64KB window then print a warning. */
	if ((unsigned long)(vga.s3.la_window << 4UL) < (unsigned long)MEM_TotalPages()) {
		if (winsz != 0x10000) // 64KB window normal for entering a DOS VM in Windows 3.1 or legacy bank switching in DOS
			LOG(LOG_MISC,LOG_WARN)("S3 warning: Window size != 64KB and address conflict with system RAM!");

		vga.lfb.page = vga.s3.la_window << 4;
		vga.lfb.addr = vga.s3.la_window << 16;
		vga.lfb.handler = NULL;
		MEM_SetLFB(0,0,NULL,NULL);
	}
	else {
		vga.lfb.page = vga.s3.la_window << 4;
		vga.lfb.addr = vga.s3.la_window << 16;
		vga.lfb.handler = &vgaph.lfb;
		MEM_SetLFB(vga.s3.la_window << 4 ,vga.vmemsize/4096, vga.lfb.handler, &vgaph.mmio);
	}
}

static bool VGA_Memory_ShutDown_init = false;

static void VGA_Memory_ShutDown(Section * /*sec*/) {
	if (vga.mem.linear_orgptr != NULL) {
		delete[] vga.mem.linear_orgptr;
		vga.mem.linear_orgptr = NULL;
		vga.mem.linear = NULL;
	}
}

void VGA_SetupMemory() {
	vga.svga.bank_read = vga.svga.bank_write = 0;
	vga.svga.bank_read_full = vga.svga.bank_write_full = 0;

    if (1 || vga.vmemsize_alloced != vga.vmemsize) {
        VGA_Memory_ShutDown(NULL);

        vga.mem.linear_orgptr = new Bit8u[vga.vmemsize+32];
        memset(vga.mem.linear_orgptr,0,vga.vmemsize+32);
        vga.mem.linear=(Bit8u*)(((uintptr_t)vga.mem.linear_orgptr + 16-1) & ~(16-1));
        vga.vmemsize_alloced = vga.vmemsize;

        /* HACK. try to avoid stale pointers */
	    vga.draw.linear_base = vga.mem.linear;
        vga.tandy.draw_base = vga.mem.linear;
        vga.tandy.mem_base = vga.mem.linear;

        /* may be related */
        VGA_SetupHandlers();
    }

	// In most cases these values stay the same. Assumptions: vmemwrap is power of 2, vmemwrap <= vmemsize
	vga.vmemwrap = vga.vmemsize;

	vga.svga.bank_read = vga.svga.bank_write = 0;
	vga.svga.bank_read_full = vga.svga.bank_write_full = 0;
	vga.svga.bank_size = 0x10000; /* most common bank size is 64K */

	if (!VGA_Memory_ShutDown_init) {
		AddExitFunction(AddExitFunctionFuncPair(VGA_Memory_ShutDown));
		VGA_Memory_ShutDown_init = true;
	}

	if (machine==MCH_PCJR) {
		/* PCJr does not have dedicated graphics memory but uses
		   conventional memory below 128k */
		//TODO map?	
	} 
}

