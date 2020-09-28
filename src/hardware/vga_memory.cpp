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
#include "pc98_cg.h"
#include "pc98_gdc.h"
#include "zipfile.h"

unsigned char pc98_pegc_mmio[0x200] = {0}; /* PC-98 memory-mapped PEGC registers at E0000h */
uint32_t pc98_pegc_banks[2] = {0x0000,0x0000}; /* bank switching offsets */

extern bool non_cga_ignore_oddeven;
extern bool non_cga_ignore_oddeven_engage;
extern bool vga_ignore_extended_memory_bit;
extern bool enable_pc98_256color_planar;
extern bool enable_pc98_256color;

#ifndef C_VGARAM_CHECKED
#define C_VGARAM_CHECKED 1
#endif

#if C_VGARAM_CHECKED
// Checked linear offset
#define CHECKED(v) ((v)&vga.mem.memmask)
// Checked planar offset (latched access)
#define CHECKED2(v) ((v)&(vga.mem.memmask>>2))
#else
#define CHECKED(v) (v)
#define CHECKED2(v) (v)
#endif

#define CHECKED3(v) ((v)&vga.mem.memmask)
#define CHECKED4(v) ((v)&(vga.mem.memmask>>2))

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
		host_writeb( off, (uint8_t)val );
	else if ( sizeof( Size ) == 2)
		host_writew( off, (uint16_t)val );
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
	}
	return 0;
}

INLINE static Bit32u ModeOperation(uint8_t val) {
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

bool pc98_pegc_linear_framebuffer_enabled(void) {
    return !!(pc98_pegc_mmio[0x102] & 1);
}

// TODO: This code may have to handle 16-bit MMIO reads
uint8_t pc98_pegc_mmio_read(unsigned int reg) {
    if (reg >= 0x200)
        return 0x00;

    return pc98_pegc_mmio[reg];
}

// TODO: This code may have to handle 16-bit MMIO writes
void pc98_pegc_mmio_write(unsigned int reg,uint8_t val) {
    if (reg >= 0x200)
        return;

    uint8_t pval = pc98_pegc_mmio[reg];

    switch (reg) {
        case 0x004: // bank 0
            pc98_pegc_banks[0] = (val & 0xFu) << 15u;
            pc98_pegc_mmio[reg] = val;
            break;
        case 0x005: // unknown (WORD size write seen by bank switched (battle skin) and LFB (DOOM, DOOM2) games)
            // ignore
            break;
        case 0x006: // bank 1
            pc98_pegc_banks[1] = (val & 0xFu) << 15u;
            pc98_pegc_mmio[reg] = val;
            break;
        case 0x007: // unknown (WORD size write seen by bank switched (battle skin) and LFB (DOOM, DOOM2) games)
            // ignore
            break;
        case 0x100: // 256-color memory access  (0=packed  1=planar)
            // WE DO NOT SUPPORT 256-planar MEMORY ACCESS AT THIS TIME!
            // FUTURE SUPPORT IS PLANNED.
            // ignore
            if (enable_pc98_256color_planar) {
                val &= 1;
                if (val & 1) {
                    pc98_gdc_vramop |= (1 << VOPBIT_PEGC_PLANAR);
                    LOG_MSG("PC-98 PEGC warning: Guest application/OS attempted to enable "
                            "256-color planar mode, which is not yet fully functional");
                }
                else {
                    pc98_gdc_vramop &= ~(1 << VOPBIT_PEGC_PLANAR);
                }
            }
            else {
                if (val & 1)
                    LOG_MSG("PC-98 PEGC warning: Guest application/OS attempted to enable "
                            "256-color planar mode, which is not enabled in your configuration");
 
                val = 0;
            }
            pc98_pegc_mmio[reg] = val;
            if ((val^pval)&1/*if bit 0 changed*/)
                VGA_SetupHandlers();
            break;
        case 0x102: // linear framebuffer (at F00000-F7FFFFh) enable/disable
            val &= 1; // as seen on real hardware: only bit 0 can be changed
            pc98_pegc_mmio[reg] = val;
            if ((val^pval)&1/*if bit 0 changed*/)
                VGA_SetupHandlers();
            // FIXME: One PC-9821 laptop seems to allow bit 0 and bit 1 to be set.
            //        What does bit 1 control?
            break;
        default:
            LOG_MSG("PC-98 PEGC warning: Unhandled write to %xh val %xh",reg+0xE0000u,val);
            break;
    }
}

/* Gonna assume that whoever maps vga memory, maps it on 32/64kb boundary */

#define VGA_PAGES		(128/4)
#define VGA_PAGE_A0		(0xA0000/4096)
#define VGA_PAGE_B0		(0xB0000/4096)
#define VGA_PAGE_B8		(0xB8000/4096)

static struct {
	Bitu base, mask;
} vgapages;

static inline Bitu VGA_Generic_Read_Handler(PhysPt planeaddr,PhysPt rawaddr,unsigned char plane) {
    const unsigned char hobit_n = ((vga.seq.memory_mode&2/*Extended Memory*/) || (vga_ignore_extended_memory_bit && IS_VGA_ARCH)) ? 16u : 14u;

    /* Sequencer Memory Mode Register (04h)
     * bits[3:3] = Chain 4 enable
     * bits[2:2] = Odd/Even Host Memory Write Addressing Disable
     * bits[1:1] = Extended memory (when EGA cards have > 64KB of RAM)
     * 
     * NTS: Real hardware experience says that despite the name, the Odd/Even bit affects reading as well */
    if (!(vga.seq.memory_mode&4) && !non_cga_ignore_oddeven_engage)/* Odd Even Host Memory Write Addressing Disable (is not set) */
        plane = (plane & ~1u) + (rawaddr & 1u);

    /* Graphics Controller: Miscellaneous Graphics Register register (06h)
     * bits[3:2] = memory map select
     * bits[1:1] = Chain Odd/Even Enable
     * bits[0:0] = Alphanumeric Mode Disable
     *
     * http://www.osdever.net/FreeVGA/vga/graphreg.htm
     *
     * When enabled, address bit A0 (bit 0) becomes bit 0 of the plane index.
     * Then when addressing VRAM A0 is replaced by a "higher order bit", which is
     * probably A14 or A16 depending on Extended Memory bit 1 in Sequencer register 04h memory mode */
    if ((vga.gfx.miscellaneous&2) && !non_cga_ignore_oddeven_engage) {/* Odd/Even enable */
        const PhysPt mask = (vga.config.compatible_chain4 ? 0u : ~0xFFFFu) + (1u << hobit_n) - 2u;
        const PhysPt hobit = (planeaddr >> hobit_n) & 1u;
        /* 1 << 14 =     0x4000
         * 1 << 14 - 1 = 0x3FFF
         * 1 << 14 - 2 = 0x3FFE
         * The point is to mask upper bit AND the LSB */
        planeaddr = (planeaddr & mask & (vga.mem.memmask >> 2u)) + hobit;
    }
    else {
        const PhysPt mask = (vga.config.compatible_chain4 ? 0u : ~0xFFFFu) + (1u << hobit_n) - 1u;
        planeaddr &= mask & (vga.mem.memmask >> 2u);
    }

    vga.latch.d=((Bit32u*)vga.mem.linear)[planeaddr];
    switch (vga.config.read_mode) {
        case 0:
            return (vga.latch.b[plane]);
        case 1:
            VGA_Latch templatch;
            templatch.d=(vga.latch.d & FillTable[vga.config.color_dont_care]) ^ FillTable[vga.config.color_compare & vga.config.color_dont_care];
            return (uint8_t)~(templatch.b[0] | templatch.b[1] | templatch.b[2] | templatch.b[3]);
    }

    return 0;
}

template <const bool chained> static inline void VGA_Generic_Write_Handler(PhysPt planeaddr,PhysPt rawaddr,uint8_t val) {
    const unsigned char hobit_n = ((vga.seq.memory_mode&2/*Extended Memory*/) || (vga_ignore_extended_memory_bit && IS_VGA_ARCH)) ? 16u : 14u;
    Bit32u mask = vga.config.full_map_mask;

    /* Sequencer Memory Mode Register (04h)
     * bits[3:3] = Chain 4 enable
     * bits[2:2] = Odd/Even Host Memory Write Addressing Disable
     * bits[1:1] = Extended memory (when EGA cards have > 64KB of RAM)
     * 
     * NTS: Real hardware experience says that despite the name, the Odd/Even bit affects reading as well */
    if (chained) {
        if (!(vga.seq.memory_mode&4) && !non_cga_ignore_oddeven_engage)/* Odd Even Host Memory Write Addressing Disable (is not set) */
            mask &= 0xFF00FFu << ((rawaddr & 1u) * 8u);
        else
            mask &= 0xFFu << ((rawaddr & 3u) * 8u);
    }
    else {
        if (!(vga.seq.memory_mode&4) && !non_cga_ignore_oddeven_engage)/* Odd Even Host Memory Write Addressing Disable (is not set) */
            mask &= 0xFF00FFu << ((rawaddr & 1u) * 8u);
    }

    /* Graphics Controller: Miscellaneous Graphics Register register (06h)
     * bits[3:2] = memory map select
     * bits[1:1] = Chain Odd/Even Enable
     * bits[0:0] = Alphanumeric Mode Disable
     *
     * http://www.osdever.net/FreeVGA/vga/graphreg.htm
     *
     * When enabled, address bit A0 (bit 0) becomes bit 0 of the plane index.
     * Then when addressing VRAM A0 is replaced by a "higher order bit", which is
     * probably A14 or A16 depending on Extended Memory bit 1 in Sequencer register 04h memory mode */
    if ((vga.gfx.miscellaneous&2) && !non_cga_ignore_oddeven_engage) {/* Odd/Even enable */
        const PhysPt mask = (vga.config.compatible_chain4 ? 0u : ~0xFFFFu) + (1u << hobit_n) - 2u;
        const PhysPt hobit = (planeaddr >> hobit_n) & 1u;
        /* 1 << 14 =     0x4000
         * 1 << 14 - 1 = 0x3FFF
         * 1 << 14 - 2 = 0x3FFE
         * The point is to mask upper bit AND the LSB */
        planeaddr = (planeaddr & mask & (vga.mem.memmask >> 2u)) + hobit;
    }
    else {
        const PhysPt mask = (vga.config.compatible_chain4 ? 0u : ~0xFFFFu) + (1u << hobit_n) - 1u;
        planeaddr &= mask & (vga.mem.memmask >> 2u);
    }

    Bit32u data=ModeOperation(val);
    VGA_Latch pixels;

    pixels.d =((Bit32u*)vga.mem.linear)[planeaddr];
    pixels.d&=~mask;
    pixels.d|=(data & mask);

    /* FIXME: A better method (I think) is to have the VGA text drawing code
     *        directly reference the font data in bitplane #2 instead of
     *        this hack */
    vga.draw.font[planeaddr] = pixels.b[2];

    ((Bit32u*)vga.mem.linear)[planeaddr]=pixels.d;
}

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
        // planar byte offset = addr & ~3u      (discard low 2 bits)
        // planer index = addr & 3u             (use low 2 bits as plane index)
        // FIXME: Does chained mode use the lower 2 bits of the CPU address or does it use the read mode select???
        return VGA_Generic_Read_Handler(addr&~3u, addr, (uint8_t)(addr&3u));
	}
	static INLINE void writeHandler8(PhysPt addr, Bitu val) {
        // planar byte offset = addr & ~3u      (discard low 2 bits)
        // planer index = addr & 3u             (use low 2 bits as plane index)
        return VGA_Generic_Write_Handler<true/*chained*/>(addr&~3u, addr, (uint8_t)val);
	}
	uint8_t readb(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_read_full;
//		addr = CHECKED(addr);
		return (uint8_t)readHandler8( addr );
	}
	uint16_t readw(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_read_full;
//		addr = CHECKED(addr);
		uint16_t ret = (uint16_t)(readHandler8( addr+0 ) << 0 );
		ret     |= (readHandler8( addr+1 ) << 8 );
		return ret;
	}
	Bit32u readd(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_read_full;
//		addr = CHECKED(addr);
		Bit32u ret = (Bit32u)(readHandler8( addr+0 ) << 0 );
		ret     |= (readHandler8( addr+1 ) << 8 );
		ret     |= (readHandler8( addr+2 ) << 16 );
		ret     |= (readHandler8( addr+3 ) << 24 );
		return ret;
	}
	void writeb(PhysPt addr, uint8_t val ) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_write_full;
//		addr = CHECKED(addr);
		writeHandler8( addr, val );
	}
	void writew(PhysPt addr,uint16_t val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_write_full;
//		addr = CHECKED(addr);
		writeHandler8( addr+0, (uint8_t)(val >> 0u) );
		writeHandler8( addr+1, (uint8_t)(val >> 8u) );
	}
	void writed(PhysPt addr,Bit32u val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_write_full;
//		addr = CHECKED(addr);
		writeHandler8( addr+0, (uint8_t)(val >> 0u) );
		writeHandler8( addr+1, (uint8_t)(val >> 8u) );
		writeHandler8( addr+2, (uint8_t)(val >> 16u) );
		writeHandler8( addr+3, (uint8_t)(val >> 24u) );
	}
};

class VGA_ET4000_ChainedVGA_Slow_Handler : public PageHandler {
public:
	VGA_ET4000_ChainedVGA_Slow_Handler() : PageHandler(PFLAG_NOCODE) {}
	static INLINE Bitu readHandler8(PhysPt addr ) {
        // planar byte offset = addr >> 2       (shift 2 bits to the right)
        // planer index = addr & 3u             (use low 2 bits as plane index)
        return VGA_Generic_Read_Handler(addr>>2u, addr, (uint8_t)(addr&3u));
	}
	static INLINE void writeHandler8(PhysPt addr, Bitu val) {
        // planar byte offset = addr >> 2       (shift 2 bits to the right)
        // planer index = addr & 3u             (use low 2 bits as plane index)
        return VGA_Generic_Write_Handler<true/*chained*/>(addr>>2u, addr, (uint8_t)val);
	}
	uint8_t readb(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_read_full;
//		addr = CHECKED(addr);
		return (uint8_t)readHandler8( addr );
	}
	uint16_t readw(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_read_full;
//		addr = CHECKED(addr);
		uint16_t ret = (uint16_t)(readHandler8( addr+0 ) << 0 );
		ret     |= (readHandler8( addr+1 ) << 8 );
		return ret;
	}
	Bit32u readd(PhysPt addr ) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_read_full;
//		addr = CHECKED(addr);
		Bit32u ret = (Bit32u)(readHandler8( addr+0 ) << 0 );
		ret     |= (readHandler8( addr+1 ) << 8 );
		ret     |= (readHandler8( addr+2 ) << 16 );
		ret     |= (readHandler8( addr+3 ) << 24 );
		return ret;
	}
	void writeb(PhysPt addr, uint8_t val ) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_write_full;
//		addr = CHECKED(addr);
		writeHandler8( addr, val );
	}
	void writew(PhysPt addr,uint16_t val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_write_full;
//		addr = CHECKED(addr);
		writeHandler8( addr+0, (uint8_t)(val >> 0u) );
		writeHandler8( addr+1, (uint8_t)(val >> 8u) );
	}
	void writed(PhysPt addr,Bit32u val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_write_full;
//		addr = CHECKED(addr);
		writeHandler8( addr+0, (uint8_t)(val >> 0u) );
		writeHandler8( addr+1, (uint8_t)(val >> 8u) );
		writeHandler8( addr+2, (uint8_t)(val >> 16u) );
		writeHandler8( addr+3, (uint8_t)(val >> 24u) );
	}
};

class VGA_UnchainedVGA_Handler : public PageHandler {
public:
	Bitu readHandler(PhysPt start) {
        return VGA_Generic_Read_Handler(start, start, vga.config.read_map_select);
	}
public:
	uint8_t readb(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_read_full;
//		addr = CHECKED2(addr);
		return (uint8_t)readHandler(addr);
	}
	uint16_t readw(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_read_full;
//		addr = CHECKED2(addr);
		uint16_t ret = (uint16_t)(readHandler(addr+0) << 0);
		ret     |= (readHandler(addr+1) << 8);
		return  ret;
	}
	Bit32u readd(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_read_full;
//		addr = CHECKED2(addr);
		Bit32u ret = (Bit32u)(readHandler(addr+0) << 0);
		ret     |= (readHandler(addr+1) << 8);
		ret     |= (readHandler(addr+2) << 16);
		ret     |= (readHandler(addr+3) << 24);
		return ret;
	}
public:
	void writeHandler(PhysPt start, uint8_t val) {
        VGA_Generic_Write_Handler<false/*chained*/>(start, start, val);
	}
public:
	VGA_UnchainedVGA_Handler() : PageHandler(PFLAG_NOCODE) {}
	void writeb(PhysPt addr,uint8_t val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_write_full;
//		addr = CHECKED2(addr);
		writeHandler(addr+0,(uint8_t)(val >> 0));
	}
	void writew(PhysPt addr,uint16_t val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_write_full;
//		addr = CHECKED2(addr);
		writeHandler(addr+0,(uint8_t)(val >> 0));
		writeHandler(addr+1,(uint8_t)(val >> 8));
	}
	void writed(PhysPt addr,Bit32u val) {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		addr += (PhysPt)vga.svga.bank_write_full;
//		addr = CHECKED2(addr);
		writeHandler(addr+0,(uint8_t)(val >> 0));
		writeHandler(addr+1,(uint8_t)(val >> 8));
		writeHandler(addr+2,(uint8_t)(val >> 16));
		writeHandler(addr+3,(uint8_t)(val >> 24));
	}
};

#include <stdio.h>

class VGA_CGATEXT_PageHandler : public PageHandler {
public:
	VGA_CGATEXT_PageHandler() {
		flags=PFLAG_NOCODE;
	}
	uint8_t readb(PhysPt addr) {
		addr = PAGING_GetPhysicalAddress(addr) & 0x3FFF;
		VGAMEM_USEC_read_delay();
		return vga.tandy.mem_base[addr];
	}
	void writeb(PhysPt addr,uint8_t val){
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

class VGA_MCGATEXT_PageHandler : public PageHandler {
public:
	VGA_MCGATEXT_PageHandler() {
		flags=PFLAG_NOCODE;
	}
	uint8_t readb(PhysPt addr) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xFFFF;
		VGAMEM_USEC_read_delay();
		return vga.tandy.mem_base[addr];
	}
	void writeb(PhysPt addr,uint8_t val){
		VGAMEM_USEC_write_delay();

		addr = PAGING_GetPhysicalAddress(addr) & 0xFFFF;
		vga.tandy.mem_base[addr] = val;
	}
};

extern uint8_t pc98_egc_srcmask[2]; /* host given (Neko: egc.srcmask) */
extern uint8_t pc98_egc_maskef[2]; /* effective (Neko: egc.mask2) */
extern uint8_t pc98_egc_mask[2]; /* host given (Neko: egc.mask) */
extern uint8_t pc98_egc_access;
extern uint8_t pc98_egc_fgc;
extern uint8_t pc98_egc_foreground_color;
extern uint8_t pc98_egc_background_color;
extern uint8_t pc98_egc_lead_plane;
extern uint8_t pc98_egc_compare_lead;
extern uint8_t pc98_egc_lightsource;
extern uint8_t pc98_egc_shiftinput;
extern uint8_t pc98_egc_regload;
extern uint8_t pc98_egc_rop;

extern bool pc98_egc_shift_descend;
extern uint8_t pc98_egc_shift_destbit;
extern uint8_t pc98_egc_shift_srcbit;
extern uint16_t pc98_egc_shift_length;

/* I don't think we necessarily need the full 4096 bit buffer
 * Neko Project II uses to render things, though that is
 * probably faster to execute. It makes it hard to make sense
 * of the code though. */
struct pc98_egc_shifter {
    pc98_egc_shifter() : decrement(false), remain(0x10), srcbit(0), dstbit(0) { }

    void reinit(void) { /* from global vars set by guest */
        decrement = pc98_egc_shift_descend;
        remain = pc98_egc_shift_length + 1; /* the register is length - 1 apparently */
        dstbit = pc98_egc_shift_destbit;
        srcbit = pc98_egc_shift_srcbit;
        bufi = bufo = decrement ? (sizeof(buffer) + 3 - (4*4)) : 0;

        if ((srcbit&7) < (dstbit&7)) {
            shft8bitr = (dstbit&7) - (srcbit&7);
            shft8bitl = 8 - shft8bitr;
        }
        else if ((srcbit&7) > (dstbit&7)) {
            shft8bitl = (srcbit&7) - (dstbit&7);
            shft8bitr = 8 - shft8bitl;
        }
        else {
            shft8bitr = 0;
            shft8bitl = 0;
        }

        shft8load = 0;
        o_srcbit = srcbit & 7;
        o_dstbit = dstbit & 7;
    }

    bool                decrement;
    uint16_t            remain;
    uint16_t            srcbit;
    uint16_t            dstbit;
    uint16_t            o_srcbit = 0;
    uint16_t            o_dstbit = 0;

    uint8_t             buffer[512] = {}; /* 4096/8 = 512 */
    uint16_t            bufi = 0, bufo = 0;

    uint8_t             shft8load = 0;
    uint8_t             shft8bitr = 0;
    uint8_t             shft8bitl = 0;

    std::string debug_status(void) {
        char tmp[512];

        sprintf(tmp,"decrement=%u remain=%u srcbit=%u dstbit=%u shf8l=%u shf8br=%u shf8bl=%u",
            decrement?1:0,
            remain,
            srcbit,
            dstbit,
            shft8load,
            shft8bitr,
            shft8bitl);

        return std::string(tmp);
    }

    template <class AWT> inline void bi(const uint16_t ofs,const AWT val) {
        size_t ip = (bufi + ofs) & (sizeof(buffer) - 1);

        for (size_t i=0;i < sizeof(AWT);) {
            buffer[ip] = (uint8_t)(val >> ((AWT)(i * 8U)));
            if ((++ip) == sizeof(buffer)) ip = 0;
            i++;
        }
    }

    template <class AWT> inline void bi_adv(void) {
        bufi += pc98_egc_shift_descend ? (sizeof(buffer) - sizeof(AWT)) : sizeof(AWT);
        bufi &= (sizeof(buffer) - 1);
    }

    template <class AWT> inline AWT bo(const uint16_t ofs) {
        size_t op = (bufo + ofs) & (sizeof(buffer) - 1);
        AWT ret = 0;

        for (size_t i=0;i < sizeof(AWT);) {
            ret += ((AWT)buffer[op]) << ((AWT)(i * 8U));
            if ((++op) == sizeof(buffer)) op = 0;
            i++;
        }

        return ret;
    }

    template <class AWT> inline void bo_adv(void) {
        bufo += pc98_egc_shift_descend ? (sizeof(buffer) - sizeof(AWT)) : sizeof(AWT);
        bufo &= (sizeof(buffer) - 1);
    }

    template <class AWT> inline void input(const AWT a,const AWT b,const AWT c,const AWT d,uint8_t odd) {
        bi<AWT>((pc98_egc_shift_descend ? (sizeof(buffer) + 1 - sizeof(AWT)) : 0) + 0,a);
        bi<AWT>((pc98_egc_shift_descend ? (sizeof(buffer) + 1 - sizeof(AWT)) : 0) + 4,b);
        bi<AWT>((pc98_egc_shift_descend ? (sizeof(buffer) + 1 - sizeof(AWT)) : 0) + 8,c);
        bi<AWT>((pc98_egc_shift_descend ? (sizeof(buffer) + 1 - sizeof(AWT)) : 0) + 12,d);

        if (shft8load <= 16) {
            bi_adv<AWT>();

            if (sizeof(AWT) == 2) {
                if (srcbit >= 8) bo_adv<uint8_t>();
                shft8load += (16 - srcbit);
                srcbit = 0;
            }
            else {
                if (srcbit >= 8)
                    srcbit -= 8;
                else {
                    shft8load += (8 - srcbit);
                    srcbit = 0;
                }
            }
        }

        *((AWT*)(pc98_egc_srcmask+odd)) = (AWT)(~0ull);
    }

    inline uint8_t dstbit_mask(void) {
        uint8_t mb;

        /* assume remain > 0 */
        if (remain >= 8)
            mb = 0xFF;
        else if (!pc98_egc_shift_descend)
            mb = 0xFF << (uint8_t)(8 - remain); /* 0x80 0xC0 0xE0 0xF0 ... */
        else
            mb = 0xFF >> (uint8_t)(8 - remain); /* 0x01 0x03 0x07 0x0F ... */

        /* assume dstbit < 8 */
        if (!pc98_egc_shift_descend)
            return mb >> (uint8_t)dstbit; /* 0xFF 0x7F 0x3F 0x1F ... */
        else
            return mb << (uint8_t)dstbit; /* 0xFF 0xFE 0xFC 0xF8 ... */
    }

    template <class AWT> inline void output(AWT &a,AWT &b,AWT &c,AWT &d,uint8_t odd,bool recursive=false) {
        if (sizeof(AWT) == 2) {
            if (shft8load < (16 - dstbit)) {
                *((AWT*)(pc98_egc_srcmask+odd)) = 0;
                return;
            }
            shft8load -= (16 - dstbit);

            /* assume odd == false and output is to even byte offset */
            if (pc98_egc_shift_descend) {
                output<uint8_t>(((uint8_t*)(&a))[1],((uint8_t*)(&b))[1],((uint8_t*)(&c))[1],((uint8_t*)(&d))[1],1,true);
                if (remain != 0) output<uint8_t>(((uint8_t*)(&a))[0],((uint8_t*)(&b))[0],((uint8_t*)(&c))[0],((uint8_t*)(&d))[0],0,true);
                else pc98_egc_srcmask[0] = 0;
            }
            else {
                output<uint8_t>(((uint8_t*)(&a))[0],((uint8_t*)(&b))[0],((uint8_t*)(&c))[0],((uint8_t*)(&d))[0],0,true);
                if (remain != 0) output<uint8_t>(((uint8_t*)(&a))[1],((uint8_t*)(&b))[1],((uint8_t*)(&c))[1],((uint8_t*)(&d))[1],1,true);
                else pc98_egc_srcmask[1] = 0;
            }

            if (remain == 0)
                reinit();

            return;
        }

        if (!recursive) {
            if (shft8load < (8 - dstbit)) {
                *((AWT*)(pc98_egc_srcmask+odd)) = 0;
                return;
            }
            shft8load -= (8 - dstbit);
        }

        if (dstbit >= 8) {
            dstbit -= 8;
            *((AWT*)(pc98_egc_srcmask+odd)) = 0;
            return;
        }

        *((AWT*)(pc98_egc_srcmask+odd)) = dstbit_mask();

        if (dstbit > 0) {
            const uint8_t bc = 8 - dstbit;

            if (remain >= bc)
                remain -= bc;
            else
                remain = 0;
        }
        else {
            if (remain >= 8)
                remain -= 8;
            else
                remain = 0;
        }

        if (o_srcbit < o_dstbit) {
            if (dstbit != 0) {
                if (pc98_egc_shift_descend) {
                    a = bo<AWT>( 0) << shft8bitr;
                    b = bo<AWT>( 4) << shft8bitr;
                    c = bo<AWT>( 8) << shft8bitr;
                    d = bo<AWT>(12) << shft8bitr;
                }
                else {
                    a = bo<AWT>( 0) >> shft8bitr;
                    b = bo<AWT>( 4) >> shft8bitr;
                    c = bo<AWT>( 8) >> shft8bitr;
                    d = bo<AWT>(12) >> shft8bitr;
                }

                dstbit = 0;
            }
            else {
                if (pc98_egc_shift_descend) {
                    bo_adv<AWT>();
                    a = (bo<AWT>( 0+1) >> shft8bitl) | (bo<AWT>( 0) << shft8bitr);
                    b = (bo<AWT>( 4+1) >> shft8bitl) | (bo<AWT>( 4) << shft8bitr);
                    c = (bo<AWT>( 8+1) >> shft8bitl) | (bo<AWT>( 8) << shft8bitr);
                    d = (bo<AWT>(12+1) >> shft8bitl) | (bo<AWT>(12) << shft8bitr);
                }
                else {
                    a = (bo<AWT>( 0) << shft8bitl) | (bo<AWT>( 0+1) >> shft8bitr);
                    b = (bo<AWT>( 4) << shft8bitl) | (bo<AWT>( 4+1) >> shft8bitr);
                    c = (bo<AWT>( 8) << shft8bitl) | (bo<AWT>( 8+1) >> shft8bitr);
                    d = (bo<AWT>(12) << shft8bitl) | (bo<AWT>(12+1) >> shft8bitr);
                    bo_adv<AWT>();
                }
            }
        }
        else if (o_srcbit > o_dstbit) {
            dstbit = 0;

            if (pc98_egc_shift_descend) {
                bo_adv<AWT>();
                a = (bo<AWT>( 0+1) >> shft8bitl) | (bo<AWT>( 0) << shft8bitr);
                b = (bo<AWT>( 4+1) >> shft8bitl) | (bo<AWT>( 4) << shft8bitr);
                c = (bo<AWT>( 8+1) >> shft8bitl) | (bo<AWT>( 8) << shft8bitr);
                d = (bo<AWT>(12+1) >> shft8bitl) | (bo<AWT>(12) << shft8bitr);
            }
            else {
                a = (bo<AWT>( 0) << shft8bitl) | (bo<AWT>( 0+1) >> shft8bitr);
                b = (bo<AWT>( 4) << shft8bitl) | (bo<AWT>( 4+1) >> shft8bitr);
                c = (bo<AWT>( 8) << shft8bitl) | (bo<AWT>( 8+1) >> shft8bitr);
                d = (bo<AWT>(12) << shft8bitl) | (bo<AWT>(12+1) >> shft8bitr);
                bo_adv<AWT>();
            }
        }
        else {
            dstbit = 0;

            a = bo<AWT>( 0);
            b = bo<AWT>( 4);
            c = bo<AWT>( 8);
            d = bo<AWT>(12);
            bo_adv<AWT>();
        }

        if (!recursive && remain == 0)
            reinit();
    }
};

egc_quad pc98_egc_src;
egc_quad pc98_egc_bgcm;
egc_quad pc98_egc_fgcm;
egc_quad pc98_egc_data;
egc_quad pc98_egc_last_vram;

pc98_egc_shifter pc98_egc_shift;

std::string pc98_egc_shift_debug_status(void) {
    return pc98_egc_shift.debug_status();
}

typedef egc_quad & (*PC98_OPEFN)(uint8_t ope, const PhysPt ad);

void pc98_egc_shift_reinit() {
    pc98_egc_shift.reinit();
}

template <class AWT> static inline void egc_fetch_planar(egc_quad &dst,const PhysPt vramoff) {
    dst[0].w = *((uint16_t*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(0)));
    dst[1].w = *((uint16_t*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(1)));
    dst[2].w = *((uint16_t*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(2)));
    dst[3].w = *((uint16_t*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(3)));
}

static egc_quad &ope_xx(uint8_t ope, const PhysPt ad) {
    (void)ad;//UNUSED
    LOG_MSG("EGC ROP 0x%2x not impl",ope);
    return pc98_egc_last_vram;
}

static egc_quad &ope_00(uint8_t ope, const PhysPt vramoff) {
	(void)vramoff;
	(void)ope;

	pc98_egc_data[0].w = 0;
	pc98_egc_data[1].w = 0;
	pc98_egc_data[2].w = 0;
	pc98_egc_data[3].w = 0;

	return pc98_egc_data;
}

static egc_quad &ope_0f(uint8_t ope, const PhysPt vramoff) {
	(void)vramoff;
	(void)ope;

	pc98_egc_data[0].w = ~pc98_egc_src[0].w;
	pc98_egc_data[1].w = ~pc98_egc_src[1].w;
	pc98_egc_data[2].w = ~pc98_egc_src[2].w;
	pc98_egc_data[3].w = ~pc98_egc_src[3].w;

	return pc98_egc_data;
}

static egc_quad &ope_ff(uint8_t ope, const PhysPt vramoff) {
	(void)vramoff;
	(void)ope;

	pc98_egc_data[0].w = (uint16_t)(~0u);
	pc98_egc_data[1].w = (uint16_t)(~0u);
	pc98_egc_data[2].w = (uint16_t)(~0u);
	pc98_egc_data[3].w = (uint16_t)(~0u);

	return pc98_egc_data;
}

static egc_quad &ope_np(uint8_t ope, const PhysPt vramoff) {
	egc_quad dst;

    egc_fetch_planar<uint16_t>(/*&*/dst,vramoff);

	pc98_egc_data[0].w = 0;
	pc98_egc_data[1].w = 0;
	pc98_egc_data[2].w = 0;
	pc98_egc_data[3].w = 0;

	if (ope & 0x80) {
        pc98_egc_data[0].w |= (pc98_egc_src[0].w & dst[0].w);
        pc98_egc_data[1].w |= (pc98_egc_src[1].w & dst[1].w);
        pc98_egc_data[2].w |= (pc98_egc_src[2].w & dst[2].w);
        pc98_egc_data[3].w |= (pc98_egc_src[3].w & dst[3].w);
    }
	if (ope & 0x20) {
        pc98_egc_data[0].w |= (pc98_egc_src[0].w & (~dst[0].w));
        pc98_egc_data[1].w |= (pc98_egc_src[1].w & (~dst[1].w));
        pc98_egc_data[2].w |= (pc98_egc_src[2].w & (~dst[2].w));
        pc98_egc_data[3].w |= (pc98_egc_src[3].w & (~dst[3].w));
	}
	if (ope & 0x08) {
        pc98_egc_data[0].w |= ((~pc98_egc_src[0].w) & dst[0].w);
        pc98_egc_data[1].w |= ((~pc98_egc_src[1].w) & dst[1].w);
        pc98_egc_data[2].w |= ((~pc98_egc_src[2].w) & dst[2].w);
        pc98_egc_data[3].w |= ((~pc98_egc_src[3].w) & dst[3].w);
	}
	if (ope & 0x02) {
        pc98_egc_data[0].w |= ((~pc98_egc_src[0].w) & (~dst[0].w));
        pc98_egc_data[1].w |= ((~pc98_egc_src[1].w) & (~dst[1].w));
        pc98_egc_data[2].w |= ((~pc98_egc_src[2].w) & (~dst[2].w));
        pc98_egc_data[3].w |= ((~pc98_egc_src[3].w) & (~dst[3].w));
	}

	(void)ope;
	(void)vramoff;
	return pc98_egc_data;
}

static egc_quad &ope_nd(uint8_t ope, const PhysPt vramoff) {
	egc_quad pat;

	switch(pc98_egc_fgc) {
		case 1:
			pat[0].w = pc98_egc_bgcm[0].w;
			pat[1].w = pc98_egc_bgcm[1].w;
			pat[2].w = pc98_egc_bgcm[2].w;
			pat[3].w = pc98_egc_bgcm[3].w;
			break;

		case 2:
			pat[0].w = pc98_egc_fgcm[0].w;
			pat[1].w = pc98_egc_fgcm[1].w;
			pat[2].w = pc98_egc_fgcm[2].w;
			pat[3].w = pc98_egc_fgcm[3].w;
			break;

		default:
			if (pc98_egc_regload & 1) {
				pat[0].w = pc98_egc_src[0].w;
				pat[1].w = pc98_egc_src[1].w;
				pat[2].w = pc98_egc_src[2].w;
				pat[3].w = pc98_egc_src[3].w;
			}
			else {
				pat[0].w = pc98_gdc_tiles[0].w;
				pat[1].w = pc98_gdc_tiles[1].w;
				pat[2].w = pc98_gdc_tiles[2].w;
				pat[3].w = pc98_gdc_tiles[3].w;
			}
			break;
	}

	pc98_egc_data[0].w = 0;
	pc98_egc_data[1].w = 0;
	pc98_egc_data[2].w = 0;
	pc98_egc_data[3].w = 0;

	if (ope & 0x80) {
        pc98_egc_data[0].w |= (pat[0].w & pc98_egc_src[0].w);
        pc98_egc_data[1].w |= (pat[1].w & pc98_egc_src[1].w);
        pc98_egc_data[2].w |= (pat[2].w & pc98_egc_src[2].w);
        pc98_egc_data[3].w |= (pat[3].w & pc98_egc_src[3].w);
    }
	if (ope & 0x40) {
        pc98_egc_data[0].w |= ((~pat[0].w) & pc98_egc_src[0].w);
        pc98_egc_data[1].w |= ((~pat[1].w) & pc98_egc_src[1].w);
        pc98_egc_data[2].w |= ((~pat[2].w) & pc98_egc_src[2].w);
        pc98_egc_data[3].w |= ((~pat[3].w) & pc98_egc_src[3].w);
    }
	if (ope & 0x08) {
        pc98_egc_data[0].w |= (pat[0].w & (~pc98_egc_src[0].w));
        pc98_egc_data[1].w |= (pat[1].w & (~pc98_egc_src[1].w));
        pc98_egc_data[2].w |= (pat[2].w & (~pc98_egc_src[2].w));
        pc98_egc_data[3].w |= (pat[3].w & (~pc98_egc_src[3].w));
    }
	if (ope & 0x04) {
        pc98_egc_data[0].w |= ((~pat[0].w) & (~pc98_egc_src[0].w));
        pc98_egc_data[1].w |= ((~pat[1].w) & (~pc98_egc_src[1].w));
        pc98_egc_data[2].w |= ((~pat[2].w) & (~pc98_egc_src[2].w));
        pc98_egc_data[3].w |= ((~pat[3].w) & (~pc98_egc_src[3].w));
    }

	(void)ope;
	(void)vramoff;
	return pc98_egc_data;
}

static egc_quad &ope_c0(uint8_t ope, const PhysPt vramoff) {
	egc_quad dst;

    /* assume: ad is word aligned */

    egc_fetch_planar<uint16_t>(/*&*/dst,vramoff);

	pc98_egc_data[0].w = pc98_egc_src[0].w & dst[0].w;
	pc98_egc_data[1].w = pc98_egc_src[1].w & dst[1].w;
	pc98_egc_data[2].w = pc98_egc_src[2].w & dst[2].w;
	pc98_egc_data[3].w = pc98_egc_src[3].w & dst[3].w;

	(void)ope;
	(void)vramoff;
	return pc98_egc_data;
}

static egc_quad &ope_f0(uint8_t ope, const PhysPt vramoff) {
	(void)ope;
	(void)vramoff;
	return pc98_egc_src;
}

static egc_quad &ope_fc(uint8_t ope, const PhysPt vramoff) {
	egc_quad dst;

    /* assume: ad is word aligned */

    egc_fetch_planar<uint16_t>(/*&*/dst,vramoff);

	pc98_egc_data[0].w  =    pc98_egc_src[0].w;
	pc98_egc_data[0].w |= ((~pc98_egc_src[0].w) & dst[0].w);
	pc98_egc_data[1].w  =    pc98_egc_src[1].w;
	pc98_egc_data[1].w |= ((~pc98_egc_src[1].w) & dst[1].w);
	pc98_egc_data[2].w  =    pc98_egc_src[2].w;
	pc98_egc_data[2].w |= ((~pc98_egc_src[2].w) & dst[2].w);
	pc98_egc_data[3].w  =    pc98_egc_src[3].w;
	pc98_egc_data[3].w |= ((~pc98_egc_src[3].w) & dst[3].w);

	(void)ope;
	(void)vramoff;
	return pc98_egc_data;
}

static egc_quad &ope_gg(uint8_t ope, const PhysPt vramoff) {
    egc_quad pat,dst;

	switch(pc98_egc_fgc) {
		case 1:
			pat[0].w = pc98_egc_bgcm[0].w;
			pat[1].w = pc98_egc_bgcm[1].w;
			pat[2].w = pc98_egc_bgcm[2].w;
			pat[3].w = pc98_egc_bgcm[3].w;
			break;

		case 2:
			pat[0].w = pc98_egc_fgcm[0].w;
			pat[1].w = pc98_egc_fgcm[1].w;
			pat[2].w = pc98_egc_fgcm[2].w;
			pat[3].w = pc98_egc_fgcm[3].w;
			break;

		default:
			if (pc98_egc_regload & 1) {
				pat[0].w = pc98_egc_src[0].w;
				pat[1].w = pc98_egc_src[1].w;
				pat[2].w = pc98_egc_src[2].w;
				pat[3].w = pc98_egc_src[3].w;
			}
			else {
				pat[0].w = pc98_gdc_tiles[0].w;
				pat[1].w = pc98_gdc_tiles[1].w;
				pat[2].w = pc98_gdc_tiles[2].w;
				pat[3].w = pc98_gdc_tiles[3].w;
			}
			break;
	}

    egc_fetch_planar<uint16_t>(/*&*/dst,vramoff);

	pc98_egc_data[0].w = 0;
	pc98_egc_data[1].w = 0;
	pc98_egc_data[2].w = 0;
	pc98_egc_data[3].w = 0;

	if (ope & 0x80) {
		pc98_egc_data[0].w |=  ( pat[0].w  &   pc98_egc_src[0].w &    dst[0].w);
		pc98_egc_data[1].w |=  ( pat[1].w  &   pc98_egc_src[1].w &    dst[1].w);
		pc98_egc_data[2].w |=  ( pat[2].w  &   pc98_egc_src[2].w &    dst[2].w);
		pc98_egc_data[3].w |=  ( pat[3].w  &   pc98_egc_src[3].w &    dst[3].w);
	}
	if (ope & 0x40) {
		pc98_egc_data[0].w |= ((~pat[0].w) &   pc98_egc_src[0].w &    dst[0].w);
		pc98_egc_data[1].w |= ((~pat[1].w) &   pc98_egc_src[1].w &    dst[1].w);
		pc98_egc_data[2].w |= ((~pat[2].w) &   pc98_egc_src[2].w &    dst[2].w);
		pc98_egc_data[3].w |= ((~pat[3].w) &   pc98_egc_src[3].w &    dst[3].w);
	}
	if (ope & 0x20) {
		pc98_egc_data[0].w |= (  pat[0].w  &   pc98_egc_src[0].w &  (~dst[0].w));
		pc98_egc_data[1].w |= (  pat[1].w  &   pc98_egc_src[1].w &  (~dst[1].w));
		pc98_egc_data[2].w |= (  pat[2].w  &   pc98_egc_src[2].w &  (~dst[2].w));
		pc98_egc_data[3].w |= (  pat[3].w  &   pc98_egc_src[3].w &  (~dst[3].w));
	}
	if (ope & 0x10) {
		pc98_egc_data[0].w |= ((~pat[0].w) &   pc98_egc_src[0].w &  (~dst[0].w));
		pc98_egc_data[1].w |= ((~pat[1].w) &   pc98_egc_src[1].w &  (~dst[1].w));
		pc98_egc_data[2].w |= ((~pat[2].w) &   pc98_egc_src[2].w &  (~dst[2].w));
		pc98_egc_data[3].w |= ((~pat[3].w) &   pc98_egc_src[3].w &  (~dst[3].w));
	}
	if (ope & 0x08) {
		pc98_egc_data[0].w |= (  pat[0].w  & (~pc98_egc_src[0].w) &   dst[0].w);
		pc98_egc_data[1].w |= (  pat[1].w  & (~pc98_egc_src[1].w) &   dst[1].w);
		pc98_egc_data[2].w |= (  pat[2].w  & (~pc98_egc_src[2].w) &   dst[2].w);
		pc98_egc_data[3].w |= (  pat[3].w  & (~pc98_egc_src[3].w) &   dst[3].w);
	}
	if (ope & 0x04) {
		pc98_egc_data[0].w |= ((~pat[0].w) & (~pc98_egc_src[0].w) &   dst[0].w);
		pc98_egc_data[1].w |= ((~pat[1].w) & (~pc98_egc_src[1].w) &   dst[1].w);
		pc98_egc_data[2].w |= ((~pat[2].w) & (~pc98_egc_src[2].w) &   dst[2].w);
		pc98_egc_data[3].w |= ((~pat[3].w) & (~pc98_egc_src[3].w) &   dst[3].w);
	}
	if (ope & 0x02) {
		pc98_egc_data[0].w |= (  pat[0].w  & (~pc98_egc_src[0].w) & (~dst[0].w));
		pc98_egc_data[1].w |= (  pat[1].w  & (~pc98_egc_src[1].w) & (~dst[1].w));
		pc98_egc_data[2].w |= (  pat[2].w  & (~pc98_egc_src[2].w) & (~dst[2].w));
		pc98_egc_data[3].w |= (  pat[3].w  & (~pc98_egc_src[3].w) & (~dst[3].w));
	}
	if (ope & 0x01) {
		pc98_egc_data[0].w |= ((~pat[0].w) & (~pc98_egc_src[0].w) & (~dst[0].w));
		pc98_egc_data[1].w |= ((~pat[1].w) & (~pc98_egc_src[1].w) & (~dst[1].w));
		pc98_egc_data[2].w |= ((~pat[2].w) & (~pc98_egc_src[2].w) & (~dst[2].w));
		pc98_egc_data[3].w |= ((~pat[3].w) & (~pc98_egc_src[3].w) & (~dst[3].w));
	}

	return pc98_egc_data;
}

static const PC98_OPEFN pc98_egc_opfn[256] = {
			ope_00, ope_xx, ope_xx, ope_np, ope_xx, ope_nd, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_nd, ope_xx, ope_np, ope_xx, ope_xx, ope_0f,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_np, ope_xx, ope_xx, ope_np, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_np, ope_xx, ope_xx, ope_np,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_nd, ope_xx, ope_xx, ope_xx, ope_xx, ope_nd, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_nd, ope_xx, ope_xx, ope_xx, ope_xx, ope_nd,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_gg, ope_xx,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_gg, ope_xx, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_gg, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_gg, ope_xx, ope_xx, ope_xx,
			ope_nd, ope_xx, ope_xx, ope_xx, ope_xx, ope_nd, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_nd, ope_xx, ope_gg, ope_xx, ope_xx, ope_nd,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_c0, ope_xx, ope_xx, ope_np, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_gg, ope_xx, ope_gg, ope_xx, ope_np, ope_gg, ope_xx, ope_np,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_xx, ope_gg, ope_xx,
			ope_f0, ope_xx, ope_xx, ope_np, ope_xx, ope_nd, ope_xx, ope_xx,
			ope_xx, ope_xx, ope_nd, ope_xx, ope_fc, ope_xx, ope_xx, ope_ff};

template <class AWT> static egc_quad &egc_ope(const PhysPt vramoff, const AWT val) {
    *((uint16_t*)pc98_egc_maskef) = *((uint16_t*)pc98_egc_mask);

    /* 4A4h
     * bits [12:11] = light source
     *    11 = invalid
     *    10 = write the contents of the palette register
     *    01 = write the result of the raster operation
     *    00 = write CPU data
     *
     * 4A2h
     * bits [14:13] = foreground, background color
     *    11 = invalid
     *    10 = foreground color
     *    01 = background color
     *    00 = pattern register
     */
    switch (pc98_egc_lightsource) {
        case 1: /* 0x0800 */
            if (pc98_egc_shiftinput) {
                pc98_egc_shift.input<AWT>(
                    val,
                    val,
                    val,
                    val,
                    vramoff&1);

                pc98_egc_shift.output<AWT>(
                    *((AWT*)(pc98_egc_src[0].b+(vramoff&1))),
                    *((AWT*)(pc98_egc_src[1].b+(vramoff&1))),
                    *((AWT*)(pc98_egc_src[2].b+(vramoff&1))),
                    *((AWT*)(pc98_egc_src[3].b+(vramoff&1))),
                    vramoff&1);
            }

            *((uint16_t*)pc98_egc_maskef) &= *((uint16_t*)pc98_egc_srcmask);
            return pc98_egc_opfn[pc98_egc_rop](pc98_egc_rop, vramoff & (~1U));
        case 2: /* 0x1000 */
            if (pc98_egc_fgc == 1)
                return pc98_egc_bgcm;
            else if (pc98_egc_fgc == 2)
                return pc98_egc_fgcm;

            if (pc98_egc_shiftinput) {
                pc98_egc_shift.input<AWT>(
                    val,
                    val,
                    val,
                    val,
                    vramoff&1);

                pc98_egc_shift.output<AWT>(
                    *((AWT*)(pc98_egc_src[0].b+(vramoff&1))),
                    *((AWT*)(pc98_egc_src[1].b+(vramoff&1))),
                    *((AWT*)(pc98_egc_src[2].b+(vramoff&1))),
                    *((AWT*)(pc98_egc_src[3].b+(vramoff&1))),
                    vramoff&1);
            }
 
            *((uint16_t*)pc98_egc_maskef) &= *((uint16_t*)pc98_egc_srcmask);
            return pc98_egc_src;
        default: {
            uint16_t tmp = (uint16_t)val;

            if (sizeof(AWT) < 2) {
                tmp &= 0xFFU;
                tmp |= tmp << 8U;
            }

            pc98_egc_data[0].w = tmp;
            pc98_egc_data[1].w = tmp;
            pc98_egc_data[2].w = tmp;
            pc98_egc_data[3].w = tmp;
            } break;
    }

    return pc98_egc_data;
}

unsigned char pc98_mem_msw_m[8] = {0};

void pc98_msw3_set_ramsize(const unsigned char b) {
    pc98_mem_msw_m[2/*MSW3*/] = b;
}

unsigned char pc98_mem_msw(unsigned char which) {
    return pc98_mem_msw_m[which&7];
}

void pc98_mem_msw_write(unsigned char which,unsigned char val) {
    LOG_MSG("WARNING: PC-98 NVRAM write to 0x%x value 0x%x, not implemented yet",which,val);
    // TODO: Add code to write NVRAM.
    //       According to documentation writing is only enabled if a register is written elsewhere to allow it.
}

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

/* A0000-A3FFF text character + attribute RAM */
/* 
 * 0xA3FE2      MSW1
 * 0xA3FE6      MSW2
 * 0xA3FEA      MSW3
 * 0xA3FEE      MSW4
 * 0xA3FF2      MSW5
 * 0xA3FF6      MSW6
 * 0xA3FFA      MSW7
 * 0xA3FFE      MSW8
 *
 * TODO: Study real hardware to determine what the bytes between the NVRAM bytes are.
 *       Are they repeats of the MSW bytes, some other value, or just 0xFF?
 */
class VGA_PC98_TEXT_PageHandler : public PageHandler {
public:
	VGA_PC98_TEXT_PageHandler() : PageHandler(PFLAG_NOCODE) {}
	uint8_t readb(PhysPt addr) {
        addr &= 0x3FFFu;

        if (addr >= 0x3FE0u)
            return pc98_mem_msw((addr >> 2u) & 7u);
        else if ((addr & 0x2001u) == 0x2001u)
            return (uint8_t)(~0u); /* Odd bytes of attribute RAM do not exist, apparently */

        return VRAM98_TEXT[addr];
    }
	void writeb(PhysPt addr,uint8_t val) {
        addr &= 0x3FFFu;

        if (addr >= 0x3FE0u)
            return pc98_mem_msw_write((addr >> 2u) & 7u,(unsigned char)val);
        else if ((addr & 0x2001u) == 0x2001u)
            return;             /* Odd bytes of attribute RAM do not exist, apparently */

        VRAM98_TEXT[addr] = (unsigned char)val;
    }
};

extern uint16_t a1_font_load_addr;

/* A4000-A4FFF character generator memory-mapped I/O */
/* 0xA4000-0xA4FFF is word-sized access to the character generator.
 *
 * Some games, though not many, appear to prefer this memory-mapped I/O
 * rather than the I/O ports.
 *
 * This fixes:
 *   - Eve Burst Error
 *
 * Also noted: Disassembling the CG functions of the BIOS on an actual
 *             PC9821 laptop reveals that the BIOS also uses this method,
 *             using REP MOVSW
 *
 * Also noted: On real hardware, A4000-A4FFF seems to latch to the CG.
 *             A5000-A5FFF seems to latch to nothing. */

/* according to real hardware, memory address does not affect char offset (port 0xA5) */
class VGA_PC98_CG_PageHandler : public PageHandler {
public:
	VGA_PC98_CG_PageHandler() : PageHandler(PFLAG_NOCODE) {}
	uint8_t readb(PhysPt addr) {
        return pc98_font_char_read(a1_font_load_addr,(addr >> 1) & 0xF,addr & 1);
    }
	void writeb(PhysPt addr,uint8_t val) {
		if ((a1_font_load_addr & 0x007E) == 0x0056 && (a1_font_load_addr & 0xFF00) != 0x0000)
			pc98_font_char_write(a1_font_load_addr,(addr >> 1) & 0xF,addr & 1,val);
		else
			LOG_MSG("A4xxx MMIO attempt to write FONT ROM char 0x%x",a1_font_load_addr);
	}
};

/* 256-color control registers, memory mapped I/O */
class VGA_PC98_256MMIO_PageHandler : public PageHandler {
public:
	VGA_PC98_256MMIO_PageHandler() : PageHandler(PFLAG_NOCODE) {}
	uint8_t readb(PhysPt addr) {
        return pc98_pegc_mmio_read(addr & 0x7FFFu);
    }
    void writeb(PhysPt addr,uint8_t val) {
        pc98_pegc_mmio_write(addr & 0x7FFFu,val);
    }
};

// A8000h-B7FFFh is 256-color planar (????)
// I don't THINK the bank switching registers have any effect. Not sure.
// However it makes sense to make it a 64KB region because 8 planes x 64KB = 512KB of RAM. Right?
// By the way real PEGC hardware seems to prefer WORD (16-bit) sized read/write aligned on WORD boundaries.
// In fact Windows 3.1's 256-color driver never uses byte-sized read/write in this planar mode.
class VGA_PC98_256Planar_PageHandler : public PageHandler {
public:
	VGA_PC98_256Planar_PageHandler() : PageHandler(PFLAG_NOCODE) {}
	uint8_t readb(PhysPt addr) {
        (void)addr;

//        LOG_MSG("PEGC 256-color planar warning: Readb from %lxh",(unsigned long)addr);
        return (uint8_t)(~0);
    }
	void writeb(PhysPt addr,uint8_t val) {
        (void)addr;
        (void)val;

//        LOG_MSG("PEGC 256-color planar warning: Writeb to %lxh val %02xh",(unsigned long)addr,(unsigned int)val);
    }
	uint16_t readw(PhysPt addr) {
        (void)addr;

//        LOG_MSG("PEGC 256-color planar warning: Readw from %lxh",(unsigned long)addr);
        return (uint16_t)(~0);
    }
	void writew(PhysPt addr,uint16_t val) {
        (void)addr;
        (void)val;

//        LOG_MSG("PEGC 256-color planar warning: Writew to %lxh val %04xh",(unsigned long)addr,(unsigned int)val);
    }
};

// A8000h is bank 0
// B0000h is bank 1
template <const unsigned int bank> class VGA_PC98_256BANK_PageHandler : public PageHandler {
public:
	VGA_PC98_256BANK_PageHandler() : PageHandler(PFLAG_NOCODE) {}
	uint8_t readb(PhysPt addr) {
        return pc98_vram_256bank_from_window(bank)[addr & 0x7FFFu];
    }
	void writeb(PhysPt addr,uint8_t val) {
        pc98_vram_256bank_from_window(bank)[addr & 0x7FFFu] = val;
    }
};

namespace pc98pgmio {

    template <class AWT> static inline void check_align(const PhysPt addr) {
#if 0
        /* DEBUG: address must be aligned to datatype.
         *        Code that calls us must enforce that or subdivide
         *        to a small datatype that can follow this rule. */
        PhysPt chk = (1UL << (sizeof(AWT) - 1)) - 1;
        /* uint8_t:  chk = 0
         * uint16_t: chk = 1
         * TODO: Do you suppose later generation PC-9821's supported DWORD size bitplane transfers?
         *       Or did NEC just give up on anything past 16-bit and focus on the SVGA side of things? */
        assert((addr&chk) == 0);
#else
        (void)addr;
#endif
    }

}

class VGA_PC98_PageHandler : public PageHandler {
public:
	VGA_PC98_PageHandler() : PageHandler(PFLAG_NOCODE) {}

    template <class AWT> static inline AWT mode8_r(const unsigned int plane,const PhysPt vramoff) {
        AWT r,b;

        b = *((AWT*)(pc98_pgraph_current_cpu_page + vramoff));
        r = b ^ *((AWT*)pc98_gdc_tiles[plane].b);

        return r;
    }

    template <class AWT> static inline void mode8_w(const unsigned int plane,const PhysPt vramoff) {
        AWT tb;

        /* Neko Project II code suggests that the first byte is repeated. */
        if (sizeof(AWT) > 1)
            tb = pc98_gdc_tiles[plane].b[0] | (pc98_gdc_tiles[plane].b[0] << 8u);
        else
            tb = pc98_gdc_tiles[plane].b[0];

        *((AWT*)(pc98_pgraph_current_cpu_page + vramoff)) = tb;
    }

    template <class AWT> static inline void modeC_w(const unsigned int plane,const PhysPt vramoff,const AWT mask,const AWT val) {
        AWT t,tb;

        /* Neko Project II code suggests that the first byte is repeated. */
        if (sizeof(AWT) > 1)
            tb = pc98_gdc_tiles[plane].b[0] | (pc98_gdc_tiles[plane].b[0] << 8u);
        else
            tb = pc98_gdc_tiles[plane].b[0];

        t  = *((AWT*)(pc98_pgraph_current_cpu_page + vramoff)) & mask;
        t |= val & tb;
        *((AWT*)(pc98_pgraph_current_cpu_page + vramoff)) = t;
    }

    template <class AWT> static inline AWT modeEGC_r(const PhysPt vramoff,const PhysPt fulloff) {
        /* assume: vramoff is even IF AWT is 16-bit wide */
        *((AWT*)(pc98_egc_last_vram[0].b+(vramoff&1))) = *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(0)));
        *((AWT*)(pc98_egc_last_vram[1].b+(vramoff&1))) = *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(1)));
        *((AWT*)(pc98_egc_last_vram[2].b+(vramoff&1))) = *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(2)));
        *((AWT*)(pc98_egc_last_vram[3].b+(vramoff&1))) = *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(3)));

        /* bits [10:10] = read source
         *    1 = shifter input is CPU write data
         *    0 = shifter input is VRAM data */
        /* Neko Project II: if ((egc.ope & 0x0400) == 0) ... */
        if (!pc98_egc_shiftinput) {
            pc98_egc_shift.input<AWT>(
                *((AWT*)(pc98_egc_last_vram[0].b+(vramoff&1))),
                *((AWT*)(pc98_egc_last_vram[1].b+(vramoff&1))),
                *((AWT*)(pc98_egc_last_vram[2].b+(vramoff&1))),
                *((AWT*)(pc98_egc_last_vram[3].b+(vramoff&1))),
                vramoff&1);

            pc98_egc_shift.output<AWT>(
                *((AWT*)(pc98_egc_src[0].b+(vramoff&1))),
                *((AWT*)(pc98_egc_src[1].b+(vramoff&1))),
                *((AWT*)(pc98_egc_src[2].b+(vramoff&1))),
                *((AWT*)(pc98_egc_src[3].b+(vramoff&1))),
                vramoff&1);
        }

        /* 0x4A4:
         * ...
         * bits [9:8] = register load (pc98_egc_regload[1:0])
         *    11 = invalid
         *    10 = load VRAM data before writing on VRAM write
         *    01 = load VRAM data into pattern/tile register on VRAM read
         *    00 = Do not change pattern/tile register
         * ...
         *
         * pc98_egc_regload = (val >> 8) & 3;
         */
        /* Neko Project II: if ((egc.ope & 0x0300) == 0x0100) ... */
        if (pc98_egc_regload & 1) { /* load VRAM data into pattern/tile... (or INVALID) */
            *((AWT*)(pc98_gdc_tiles[0].b+(vramoff&1))) = *((AWT*)(pc98_egc_last_vram[0].b+(vramoff&1)));
            *((AWT*)(pc98_gdc_tiles[1].b+(vramoff&1))) = *((AWT*)(pc98_egc_last_vram[1].b+(vramoff&1)));
            *((AWT*)(pc98_gdc_tiles[2].b+(vramoff&1))) = *((AWT*)(pc98_egc_last_vram[2].b+(vramoff&1)));
            *((AWT*)(pc98_gdc_tiles[3].b+(vramoff&1))) = *((AWT*)(pc98_egc_last_vram[3].b+(vramoff&1)));
        }

        /* 0x4A4:
         * bits [13:13] = 0=compare lead plane  1=don't
         *
         * bits [10:10] = read source
         *    1 = shifter input is CPU write data
         *    0 = shifter input is VRAM data */
        if (pc98_egc_compare_lead) {
            if (!pc98_egc_shiftinput)
                return *((AWT*)(pc98_egc_src[pc98_egc_lead_plane&3].b));
            else
                return *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+((pc98_egc_lead_plane&3)*PC98_VRAM_BITPLANE_SIZE)));
        }

        return *((AWT*)(pc98_pgraph_current_cpu_page+fulloff));
    }

    template <class AWT> static inline void modeEGC_w(const PhysPt vramoff,const AWT val) {
        /* assume: vramoff is even IF AWT is 16-bit wide */

        /* 0x4A4:
         * ...
         * bits [9:8] = register load (pc98_egc_regload[1:0])
         *    11 = invalid
         *    10 = load VRAM data before writing on VRAM write
         *    01 = load VRAM data into pattern/tile register on VRAM read
         *    00 = Do not change pattern/tile register
         * ...
         * pc98_egc_regload = (val >> 8) & 3;
         */
        /* Neko Project II: if ((egc.ope & 0x0300) == 0x0200) ... */
        if (pc98_egc_regload & 2) { /* load VRAM data before writing on VRAM write (or INVALID) */
            *((AWT*)(pc98_gdc_tiles[0].b+(vramoff&1))) = *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(0)));
            *((AWT*)(pc98_gdc_tiles[1].b+(vramoff&1))) = *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(1)));
            *((AWT*)(pc98_gdc_tiles[2].b+(vramoff&1))) = *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(2)));
            *((AWT*)(pc98_gdc_tiles[3].b+(vramoff&1))) = *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(3)));
        }

        egc_quad &ropdata = egc_ope<AWT>(vramoff, val);

        const AWT accmask = *((AWT*)(pc98_egc_maskef+(vramoff&1)));

        if (accmask != 0) {
            if (!(pc98_egc_access & 1)) {
                *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(0))) &= ~accmask;
                *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(0))) |=  accmask & *((AWT*)(ropdata[0].b+(vramoff&1)));
            }
            if (!(pc98_egc_access & 2)) {
                *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(1))) &= ~accmask;
                *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(1))) |=  accmask & *((AWT*)(ropdata[1].b+(vramoff&1)));
            }
            if (!(pc98_egc_access & 4)) {
                *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(2))) &= ~accmask;
                *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(2))) |=  accmask & *((AWT*)(ropdata[2].b+(vramoff&1)));
            }
            if (!(pc98_egc_access & 8)) {
                *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(3))) &= ~accmask;
                *((AWT*)(pc98_pgraph_current_cpu_page+vramoff+pc98_pgram_bitplane_offset(3))) |=  accmask & *((AWT*)(ropdata[3].b+(vramoff&1)));
            }
        }
    }

    template <class AWT> AWT readc(PhysPt addr) {
        pc98pgmio::check_align<AWT>(addr);

        unsigned int plane = ((addr >> 15u) + 3u) & 3u;
        addr &= 0x7FFF;

        /* reminder:
         *
         * bit 1: VOPBIT_EGC
         * bit 0: VOPBIT_ACCESS
         * From GRGC bits:
         * bit 3: VOPBIT_GRCG  1=GRGC active  0=GRGC invalid  (from bit 7)
         * bit 2: VOPBIT_GRCG  1=Read/Modify/Write when writing  0=TCR mode at read, TDW mode at write  (from bit 6) */
        switch (pc98_gdc_vramop & 0xF) {
            case 0x00:
            case 0x01:
            case 0x02:
            case 0x03:
            case 0x04:
            case 0x05:
            case 0x06:
            case 0x07:
            case 0x0C:
            case 0x0D:
                return *((AWT*)(pc98_pgraph_current_cpu_page+addr+pc98_pgram_bitplane_offset(plane)));
            case 0x08: /* TCR/TDW */
            case 0x09:
                {
                    AWT r = 0;

                    /* this reads multiple bitplanes at once */
                    if (!(pc98_gdc_modereg & 1)) // blue channel
                        r |= mode8_r<AWT>(/*plane*/0,addr + pc98_pgram_bitplane_offset(0));

                    if (!(pc98_gdc_modereg & 2)) // red channel
                        r |= mode8_r<AWT>(/*plane*/1,addr + pc98_pgram_bitplane_offset(1));

                    if (!(pc98_gdc_modereg & 4)) // green channel
                        r |= mode8_r<AWT>(/*plane*/2,addr + pc98_pgram_bitplane_offset(2));

                    if (!(pc98_gdc_modereg & 8)) // extended channel
                        r |= mode8_r<AWT>(/*plane*/3,addr + pc98_pgram_bitplane_offset(3));

                    /* NTS: Apparently returning this value correctly really matters to the
                     *      sprite engine in "Edge", else visual errors occur. */
                    return ~r;
                }
            case 0x0A: /* EGC read */
            case 0x0B:
            case 0x0E:
            case 0x0F:
                /* this reads multiple bitplanes at once */
                return modeEGC_r<AWT>(addr,addr);
            default: /* should not happen */
                break;
        }

		return (AWT)(~0ull);
	}

	template <class AWT> void writec(PhysPt addr,AWT val) {
        pc98pgmio::check_align<AWT>(addr);

        unsigned int plane = ((addr >> 15u) + 3u) & 3u;
        addr &= 0x7FFF;

        /* reminder:
         *
         * bit 1: VOPBIT_EGC
         * bit 0: VOPBIT_ACCESS
         * From GRGC bits:
         * bit 3: VOPBIT_GRCG  1=GRGC active  0=GRGC invalid  (from bit 7)
         * bit 2: VOPBIT_GRCG  1=Read/Modify/Write when writing  0=TCR mode at read, TDW mode at write  (from bit 6) */
        switch (pc98_gdc_vramop & 0xF) {
            case 0x00:
            case 0x01:
            case 0x02:
            case 0x03:
            case 0x04:
            case 0x05:
            case 0x06:
            case 0x07:
                *((AWT*)(pc98_pgraph_current_cpu_page+addr+pc98_pgram_bitplane_offset(plane))) = val;
                break;
            case 0x08:  /* TCR/TDW write tile data, no masking */
            case 0x09:
                {
                    /* this writes to multiple bitplanes at once.
                     * notice that the value written has no meaning, only the tile data and memory address. */
                    if (!(pc98_gdc_modereg & 1)) // blue channel
                        mode8_w<AWT>(0/*plane*/,addr + pc98_pgram_bitplane_offset(0));

                    if (!(pc98_gdc_modereg & 2)) // red channel
                        mode8_w<AWT>(1/*plane*/,addr + pc98_pgram_bitplane_offset(1));

                    if (!(pc98_gdc_modereg & 4)) // green channel
                        mode8_w<AWT>(2/*plane*/,addr + pc98_pgram_bitplane_offset(2));

                    if (!(pc98_gdc_modereg & 8)) // extended channel
                        mode8_w<AWT>(3/*plane*/,addr + pc98_pgram_bitplane_offset(3));
                }
                break;
            case 0x0C:  /* read/modify/write from tile with masking */
            case 0x0D:  /* a lot of PC-98 games seem to rely on this for sprite rendering */
                {
                    const AWT mask = ~val;

                    /* this writes to multiple bitplanes at once */
                    if (!(pc98_gdc_modereg & 1)) // blue channel
                        modeC_w<AWT>(0/*plane*/,addr + pc98_pgram_bitplane_offset(0),mask,val);

                    if (!(pc98_gdc_modereg & 2)) // red channel
                        modeC_w<AWT>(1/*plane*/,addr + pc98_pgram_bitplane_offset(1),mask,val);

                    if (!(pc98_gdc_modereg & 4)) // green channel
                        modeC_w<AWT>(2/*plane*/,addr + pc98_pgram_bitplane_offset(2),mask,val);

                    if (!(pc98_gdc_modereg & 8)) // extended channel
                        modeC_w<AWT>(3/*plane*/,addr + pc98_pgram_bitplane_offset(3),mask,val);
                }
                break;
            case 0x0A: /* EGC write */
            case 0x0B:
            case 0x0E:
            case 0x0F:
                /* this reads multiple bitplanes at once */
                modeEGC_w<AWT>(addr,val);
                break;
            default: /* Should not happen */
                break;
        }
	}

    /* byte-wise */
	uint8_t readb(PhysPt addr) {
        return readc<uint8_t>( PAGING_GetPhysicalAddress(addr) );
    }
	void writeb(PhysPt addr,uint8_t val) {
        writec<uint8_t>( PAGING_GetPhysicalAddress(addr), val );
    }

    /* word-wise.
     * in the style of the 8086, non-word-aligned I/O is split into byte I/O */
	uint16_t readw(PhysPt addr) {
        addr = PAGING_GetPhysicalAddress(addr);
        if (!(addr & 1)) /* if WORD aligned */
            return readc<uint16_t>(addr);
        else {
            return   (unsigned int)readc<uint8_t>(addr+0U) +
                    ((unsigned int)readc<uint8_t>(addr+1U) << 8u);
        }
    }
	void writew(PhysPt addr,uint16_t val) {
        addr = PAGING_GetPhysicalAddress(addr);
        if (!(addr & 1)) /* if WORD aligned */
            writec<uint16_t>(addr,val);
        else {
            writec<uint8_t>(addr+0,(uint8_t)val);
            writec<uint8_t>(addr+1,(uint8_t)(val >> 8U));
        }
    }
};

class VGA_PC98_LFB_Handler : public PageHandler {
public:
	VGA_PC98_LFB_Handler() : PageHandler(PFLAG_READABLE|PFLAG_WRITEABLE|PFLAG_NOCODE) {}
	HostPt GetHostReadPt(Bitu phys_page) {
		return &vga.mem.linear[(phys_page&0x7F)*4096 + PC98_VRAM_GRAPHICS_OFFSET]; /* 512KB mapping */
	}
	HostPt GetHostWritePt(Bitu phys_page) {
		return &vga.mem.linear[(phys_page&0x7F)*4096 + PC98_VRAM_GRAPHICS_OFFSET]; /* 512KB mapping */
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
		Bits delaycyc = (Bits)(CPU_CycleMax/((cpu_cycles_count_t)(1024/2.80)));
		if(GCC_UNLIKELY(CPU_Cycles < 3*delaycyc)) delaycyc=0;
		CPU_Cycles -= delaycyc;
		CPU_IODelayRemoved += delaycyc;
	}

	uint8_t readb(PhysPt addr) {
		delay();
		return vga.tandy.mem_base[(addr - 0xb8000) & 0x3FFF];
	}
	void writeb(PhysPt addr,uint8_t val){
		delay();
		vga.tandy.mem_base[(addr - 0xb8000) & 0x3FFF] = val;
	}
	
};

class VGA_LFB_Handler : public PageHandler {
public:
	VGA_LFB_Handler() : PageHandler(PFLAG_READABLE|PFLAG_WRITEABLE|PFLAG_NOCODE) {}
	HostPt GetHostReadPt( Bitu phys_page ) {
		phys_page -= vga.lfb.page;
		phys_page &= (vga.mem.memsize >> 12) - 1;
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
	void writeb(PhysPt addr,uint8_t val) {
		VGAMEM_USEC_write_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		XGA_Write(port, val, 1);
	}
	void writew(PhysPt addr,uint16_t val) {
		VGAMEM_USEC_write_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		XGA_Write(port, val, 2);
	}
	void writed(PhysPt addr,Bit32u val) {
		VGAMEM_USEC_write_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		XGA_Write(port, val, 4);
	}

	uint8_t readb(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		return (uint8_t)XGA_Read(port, 1);
	}
	uint16_t readw(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		return (uint16_t)XGA_Read(port, 2);
	}
	Bit32u readd(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		return (Bit32u)XGA_Read(port, 4);
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
	void writeHandler(PhysPt start, uint8_t val) {
		vga.tandy.mem_base[ start ] = val;
#ifdef DIJDIJD
		Bit32u data=ModeOperation(val);
		/* Update video memory and the pixel buffer */
		VGA_Latch pixels;
		pixels.d=((Bit32u*)vga.mem.linear)[start];
		pixels.d&=vga.config.full_not_map_mask;
		pixels.d|=(data & vga.config.full_map_mask);
		((Bit32u*)vga.mem.linear)[start]=pixels.d;
		uint8_t * write_pixels=&vga.mem.linear[VGA_CACHE_OFFSET+(start<<3)];

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
	uint8_t readHandler(PhysPt start) {
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
			PhysPt phys_page = addr >> 12;
			//test for a unaliged bank, then replicate 2x16kb
			if (vga.tandy.mem_bank & 1) 
				phys_page&=0x03;
			return ( phys_page * 4096 ) + ( addr & 0x0FFF );
		}
		return ( (PAGING_GetPhysicalAddress(addr) & 0xffff) - 0x8000 ) & ( 32*1024-1 );
	}

	void writeb(PhysPt addr,uint8_t val) {
		VGAMEM_USEC_write_delay();
		addr = wrAddr( addr );
		Bitu plane = vga.mode==M_AMSTRAD ? vga.amstrad.write_plane : 0x01; // 0x0F?
		if( plane & 0x08 ) writeHandler<false>(addr+49152,(uint8_t)(val >> 0));
		if( plane & 0x04 ) writeHandler<false>(addr+32768,(uint8_t)(val >> 0));
		if( plane & 0x02 ) writeHandler<false>(addr+16384,(uint8_t)(val >> 0));
		if( plane & 0x01 ) writeHandler<false>(addr+0,(uint8_t)(val >> 0));
	}
	void writew(PhysPt addr,uint16_t val) {
		VGAMEM_USEC_write_delay();
		addr = wrAddr( addr );
		Bitu plane = vga.mode==M_AMSTRAD ? vga.amstrad.write_plane : 0x01; // 0x0F?
		if( plane & 0x01 )
		{
			writeHandler<false>(addr+0,(uint8_t)(val >> 0));
			writeHandler<false>(addr+1,(uint8_t)(val >> 8));
		}
		addr += 16384;
		if( plane & 0x02 )
		{
			writeHandler<false>(addr+0,(uint8_t)(val >> 0));
			writeHandler<false>(addr+1,(uint8_t)(val >> 8));
		}
		addr += 16384;
		if( plane & 0x04 )
		{
			writeHandler<false>(addr+0,(uint8_t)(val >> 0));
			writeHandler<false>(addr+1,(uint8_t)(val >> 8));
		}
		addr += 16384;
		if( plane & 0x08 )
		{
			writeHandler<false>(addr+0,(uint8_t)(val >> 0));
			writeHandler<false>(addr+1,(uint8_t)(val >> 8));
		}

	}
	void writed(PhysPt addr,Bit32u val) {
		VGAMEM_USEC_write_delay();
		addr = wrAddr( addr );
		Bitu plane = vga.mode==M_AMSTRAD ? vga.amstrad.write_plane : 0x01; // 0x0F?
		if( plane & 0x01 )
		{
			writeHandler<false>(addr+0,(uint8_t)(val >> 0));
			writeHandler<false>(addr+1,(uint8_t)(val >> 8));
			writeHandler<false>(addr+2,(uint8_t)(val >> 16));
			writeHandler<false>(addr+3,(uint8_t)(val >> 24));
		}
		addr += 16384;
		if( plane & 0x02 )
		{
			writeHandler<false>(addr+0,(uint8_t)(val >> 0));
			writeHandler<false>(addr+1,(uint8_t)(val >> 8));
			writeHandler<false>(addr+2,(uint8_t)(val >> 16));
			writeHandler<false>(addr+3,(uint8_t)(val >> 24));
		}
		addr += 16384;
		if( plane & 0x04 )
		{
			writeHandler<false>(addr+0,(uint8_t)(val >> 0));
			writeHandler<false>(addr+1,(uint8_t)(val >> 8));
			writeHandler<false>(addr+2,(uint8_t)(val >> 16));
			writeHandler<false>(addr+3,(uint8_t)(val >> 24));
		}
		addr += 16384;
		if( plane & 0x08 )
		{
			writeHandler<false>(addr+0,(uint8_t)(val >> 0));
			writeHandler<false>(addr+1,(uint8_t)(val >> 8));
			writeHandler<false>(addr+2,(uint8_t)(val >> 16));
			writeHandler<false>(addr+3,(uint8_t)(val >> 24));
		}

	}
	uint8_t readb(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = wrAddr( addr ) + ( vga.amstrad.read_plane * 16384u );
		addr &= (64u*1024u-1u);
		return readHandler(addr);
	}
	uint16_t readw(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = wrAddr( addr ) + ( vga.amstrad.read_plane * 16384u );
		addr &= (64u*1024u-1u);
		return 
			(readHandler(addr+0) << 0u) |
			(readHandler(addr+1) << 8u);
	}
	Bit32u readd(PhysPt addr) {
		VGAMEM_USEC_read_delay();
		addr = wrAddr( addr ) + ( vga.amstrad.read_plane * 16384u );
		addr &= (64u*1024u-1u);
		return 
			(Bit32u)(readHandler(addr+0) << 0u)  |
			(Bit32u)(readHandler(addr+1) << 8u)  |
			(Bit32u)(readHandler(addr+2) << 16u) |
			(Bit32u)(readHandler(addr+3) << 24u);
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
        (void)phys_page;//UNUSED
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
	uint8_t readb(PhysPt /*addr*/) {
//		LOG(LOG_VGA, LOG_NORMAL ) ( "Read from empty memory space at %x", addr );
		return 0xff;
	} 
	void writeb(PhysPt /*addr*/,uint8_t /*val*/) {
//		LOG(LOG_VGA, LOG_NORMAL ) ( "Write %x to empty memory space at %x", val, addr );
	}
};

static struct vg {
	VGA_PC98_LFB_Handler		map_lfb_pc98;
	VGA_Map_Handler				map;
	VGA_Slow_CGA_Handler		slow;
//	VGA_TEXT_PageHandler		text;
	VGA_CGATEXT_PageHandler		cgatext;
	VGA_MCGATEXT_PageHandler	mcgatext;
	VGA_TANDY_PageHandler		tandy;
//	VGA_ChainedEGA_Handler		cega;
//	VGA_ChainedVGA_Handler		cvga;
	VGA_ChainedVGA_Slow_Handler	cvga_slow;
//	VGA_ET4000_ChainedVGA_Handler		cvga_et4000;
	VGA_ET4000_ChainedVGA_Slow_Handler	cvga_et4000_slow;
//	VGA_UnchainedEGA_Handler	uega;
	VGA_UnchainedVGA_Handler	uvga;
	VGA_PCJR_Handler			pcjr;
	VGA_HERC_Handler			herc;
//	VGA_LIN4_Handler			lin4;
	VGA_LFB_Handler				lfb;
	VGA_MMIO_Handler			mmio;
	VGA_AMS_Handler				ams;
    VGA_PC98_PageHandler        pc98;
    VGA_PC98_TEXT_PageHandler   pc98_text;
    VGA_PC98_CG_PageHandler     pc98_cg;
    VGA_PC98_256MMIO_PageHandler pc98_256mmio;
    VGA_PC98_256BANK_PageHandler<0> pc98_256bank0;
    VGA_PC98_256BANK_PageHandler<1> pc98_256bank1;
    VGA_PC98_256Planar_PageHandler pc98_256planar;
	VGA_Empty_Handler			empty;
} vgaph;

void VGA_ChangedBank(void) {
	VGA_SetupHandlers();
}

void MEM_ResetPageHandler_Unmapped(Bitu phys_page, Bitu pages);
void MEM_ResetPageHandler_RAM(Bitu phys_page, Bitu pages);

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
	case MCH_MCGA://Based on real hardware, A0000-BFFFF is the 64KB of RAM mapped twice
		MEM_SetPageHandler( VGA_PAGE_A0, 16, &vgaph.mcgatext );     // A0000-AFFFF is the 64KB of video RAM
        MEM_ResetPageHandler_Unmapped( VGA_PAGE_B0, 8 );            // B0000-B7FFF is unmapped
		MEM_SetPageHandler( VGA_PAGE_B8, 8, &vgaph.mcgatext );      // B8000-BFFFF is the last 32KB half of video RAM, alias
		goto range_done;
	case MCH_PCJR:
		MEM_SetPageHandler( VGA_PAGE_A0, 16, &vgaph.empty );
		MEM_SetPageHandler( VGA_PAGE_B0, 8, &vgaph.empty );
		MEM_SetPageHandler( VGA_PAGE_B8, 8, &vgaph.pcjr );
		goto range_done;
	case MCH_MDA:
	case MCH_HERC:
		MEM_SetPageHandler( VGA_PAGE_A0, 16, &vgaph.empty );
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
			MEM_SetPageHandler( VGA_PAGE_B8, 8, &vgaph.tandy );
		}
		goto range_done;
//		MEM_SetPageHandler(vga.tandy.mem_bank<<2,vga.tandy.is_32k_mode ? 0x08 : 0x04,range_handler);
	case MCH_AMSTRAD: // Memory handler.
		MEM_SetPageHandler( 0xb8, 8, &vgaph.ams );
		goto range_done;
	case EGAVGA_ARCH_CASE:
        break;
    case PC98_ARCH_CASE:
        MEM_SetPageHandler(             VGA_PAGE_A0 + 0x00, 0x02, &vgaph.pc98_text );/* A0000-A1FFFh text layer, character data */
        MEM_SetPageHandler(             VGA_PAGE_A0 + 0x02, 0x02, &vgaph.pc98_text );/* A2000-A3FFFh text layer, attribute data + non-volatile RAM */
        MEM_SetPageHandler(             VGA_PAGE_A0 + 0x04, 0x01, &vgaph.pc98_cg );  /* A4000-A4FFFh character generator memory-mapped I/O */
        MEM_ResetPageHandler_Unmapped(  VGA_PAGE_A0 + 0x05, 0x03);                   /* A5000-A7FFFh not mapped */

        if (pc98_gdc_vramop & (1 << VOPBIT_VGA)) {
            if (pc98_gdc_vramop & (1 << VOPBIT_PEGC_PLANAR)) {
                MEM_SetPageHandler(             VGA_PAGE_A0 + 0x08, 0x10, &vgaph.pc98_256planar );/* A8000-B7FFFh planar graphics (???) */
                MEM_ResetPageHandler_Unmapped(  VGA_PAGE_A0 + 0x18, 0x08);                        /* B8000-BFFFFh graphics layer, not mapped */
            }
            else {
                MEM_SetPageHandler(             VGA_PAGE_A0 + 0x08, 0x08, &vgaph.pc98_256bank0 );/* A8000-AFFFFh graphics layer, bank 0 */
                MEM_SetPageHandler(             VGA_PAGE_A0 + 0x10, 0x08, &vgaph.pc98_256bank1 );/* B0000-B7FFFh graphics layer, bank 1 */
                MEM_ResetPageHandler_Unmapped(  VGA_PAGE_A0 + 0x18, 0x08);                       /* B8000-BFFFFh graphics layer, not mapped */
            }
        }
        else {
            MEM_SetPageHandler(             VGA_PAGE_A0 + 0x08, 0x08, &vgaph.pc98 );/* A8000-AFFFFh graphics layer, B bitplane */
            MEM_SetPageHandler(             VGA_PAGE_A0 + 0x10, 0x08, &vgaph.pc98 );/* B0000-B7FFFh graphics layer, R bitplane */
            MEM_SetPageHandler(             VGA_PAGE_A0 + 0x18, 0x08, &vgaph.pc98 );/* B8000-BFFFFh graphics layer, G bitplane */
        }

        /* E0000-E7FFFh graphics layer
         *  - In 8-color mode, E0000-E7FFFh is not mapped
         *  - In 16-color mode, E0000-E7FFFh is the 4th bitplane (E)
         *  - In 256-color mode, E0000-E7FFFh is memory-mapped I/O that controls the 256-color mode */
        if (pc98_gdc_vramop & (1 << VOPBIT_VGA))
            MEM_SetPageHandler(0xE0, 8, &vgaph.pc98_256mmio );
        else if (pc98_gdc_vramop & (1 << VOPBIT_ANALOG))
            MEM_SetPageHandler(0xE0, 8, &vgaph.pc98 );
        else
            MEM_ResetPageHandler_Unmapped(0xE0, 8);

        // TODO: What about PC-9821 systems with more than 15MB of RAM? Do they maintain a "hole"
        //       in memory for this linear framebuffer? Intel motherboard chipsets of that era do
        //       support a 15MB memory hole.
        if (MEM_TotalPages() <= 0xF00/*FIXME*/) {
            /* F00000-FF7FFFh linear framebuffer (256-packed)
             *  - Does not exist except in 256-color mode.
             *  - Switching from 256-color mode immediately unmaps this linear framebuffer.
             *  - Switching to 256-color mode will immediately map the linear framebuffer if the enable bit is set in the PEGC MMIO registers */
            if ((pc98_gdc_vramop & (1 << VOPBIT_VGA)) && pc98_pegc_linear_framebuffer_enabled())
                MEM_SetPageHandler(0xF00, 512/*kb*/ / 4/*kb*/, &vgaph.map_lfb_pc98 );
            else
                MEM_ResetPageHandler_Unmapped(0xF00, 512/*kb*/ / 4/*kb*/);
        }

        goto range_done;
	default:
		LOG_MSG("Illegal machine type %d", machine );
		return;
	}

	/* This should be vga only */
	switch (vga.mode) {
	case M_ERROR:
	default:
		return;
	case M_LIN15:
	case M_LIN16:
	case M_LIN24:
	case M_LIN32:
    case M_PACKED4:
		newHandler = &vgaph.map;
		break;
	case M_TEXT:
	case M_CGA2:
	case M_CGA4:
        /* EGA/VGA emulate CGA modes as chained */
        /* fall through */
	case M_LIN8:
	case M_LIN4:
	case M_VGA:
	case M_EGA:
        if (vga.config.chained) {
            if (vga.config.compatible_chain4) {
                /* NTS: ET4000AX cards appear to have a different chain4 implementation from everyone else:
                 *      the planar memory byte address is address >> 2 and bits A0-A1 select the plane,
                 *      where all other clones I've tested seem to write planar memory byte (address & ~3)
                 *      (one byte per 4 bytes) and bits A0-A1 select the plane. */
                /* FIXME: Different chain4 implementation on ET4000 noted---is it true also for ET3000? */
                if (svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K)
                    newHandler = &vgaph.cvga_et4000_slow;
                else
                    newHandler = &vgaph.cvga_slow;
            }
            else {
                /* this is needed for SVGA modes (Paradise, Tseng, S3) because SVGA
                 * modes do NOT use the chain4 configuration */
                newHandler = &vgaph.map;
            }
        } else {
            newHandler = &vgaph.uvga;
        }
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
            case SVGA_TsengET4K:
                vgapages.mask = 0x1ffff & vga.mem.memmask;
                break;
                /* NTS: Looking at the official ET4000 programming guide, it does in fact support the full 128KB */
            case SVGA_S3Trio:
            default:
                vgapages.mask = 0xffff & vga.mem.memmask;
                break;
		}
		MEM_SetPageHandler(VGA_PAGE_A0, 32, newHandler );
		break;
	case 1:
		vgapages.base = VGA_PAGE_A0;
		vgapages.mask = 0xffff & vga.mem.memmask;
		MEM_SetPageHandler( VGA_PAGE_A0, 16, newHandler );
		MEM_SetPageHandler( VGA_PAGE_B0, 16, &vgaph.empty );
		break;
	case 2:
		vgapages.base = VGA_PAGE_B0;
		vgapages.mask = 0x7fff & vga.mem.memmask;
		MEM_SetPageHandler( VGA_PAGE_B0, 8, newHandler );
		MEM_SetPageHandler( VGA_PAGE_A0, 16, &vgaph.empty );
		MEM_SetPageHandler( VGA_PAGE_B8, 8, &vgaph.empty );
        break;
	case 3:
		vgapages.base = VGA_PAGE_B8;
		vgapages.mask = 0x7fff & vga.mem.memmask;
		MEM_SetPageHandler( VGA_PAGE_B8, 8, newHandler );
		MEM_SetPageHandler( VGA_PAGE_A0, 16, &vgaph.empty );
		MEM_SetPageHandler( VGA_PAGE_B0, 8, &vgaph.empty );
        break;
	}
	if(svgaCard == SVGA_S3Trio && (vga.s3.ext_mem_ctrl & 0x10))
		MEM_SetPageHandler(VGA_PAGE_A0, 16, &vgaph.mmio);

    non_cga_ignore_oddeven_engage = (non_cga_ignore_oddeven && !(vga.mode == M_TEXT || vga.mode == M_CGA2 || vga.mode == M_CGA4));

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

    /* The LFB register has an enable bit */
    if (!(vga.s3.reg_58 & 0x10)) {
        vga.lfb.page = (unsigned int)vga.s3.la_window << 4u;
        vga.lfb.addr = (unsigned int)vga.s3.la_window << 16u;
        vga.lfb.handler = NULL;
        MEM_SetLFB(0,0,NULL,NULL);
    }
    /* if the DOS application or Windows 3.1 driver attempts to put the linear framebuffer
	 * below the top of memory, then we're probably entering a DOS VM and it's probably
	 * a 64KB window. If it's not a 64KB window then print a warning. */
    else if ((unsigned long)(vga.s3.la_window << 4UL) < (unsigned long)MEM_TotalPages()) {
		if (winsz != 0x10000) // 64KB window normal for entering a DOS VM in Windows 3.1 or legacy bank switching in DOS
			LOG(LOG_MISC,LOG_WARN)("S3 warning: Window size != 64KB and address conflict with system RAM!");

		vga.lfb.page = (unsigned int)vga.s3.la_window << 4u;
		vga.lfb.addr = (unsigned int)vga.s3.la_window << 16u;
		vga.lfb.handler = NULL;
		MEM_SetLFB(0,0,NULL,NULL);
	}
	else {
		vga.lfb.page = (unsigned int)vga.s3.la_window << 4u;
		vga.lfb.addr = (unsigned int)vga.s3.la_window << 16u;
		vga.lfb.handler = &vgaph.lfb;
		MEM_SetLFB((unsigned int)vga.s3.la_window << 4u,(unsigned int)vga.mem.memsize/4096u, vga.lfb.handler, &vgaph.mmio);
	}
}

static bool VGA_Memory_ShutDown_init = false;

static void VGA_Memory_ShutDown(Section * /*sec*/) {
	MEM_SetPageHandler(VGA_PAGE_A0,32,&vgaph.empty);
	PAGING_ClearTLB();

	if (vga.mem.linear_orgptr != NULL) {
		delete[] vga.mem.linear_orgptr;
		vga.mem.linear_orgptr = NULL;
		vga.mem.linear = NULL;
	}
}

void VGA_SetupMemory() {
	vga.svga.bank_read = vga.svga.bank_write = 0;
	vga.svga.bank_read_full = vga.svga.bank_write_full = 0;

    if (vga.mem.linear == NULL) {
        VGA_Memory_ShutDown(NULL);

        vga.mem.linear_orgptr = new uint8_t[vga.mem.memsize+32u];
        memset(vga.mem.linear_orgptr,0,vga.mem.memsize+32u);
        vga.mem.linear=(uint8_t*)(((uintptr_t)vga.mem.linear_orgptr + 16ull-1ull) & ~(16ull-1ull));

        /* HACK. try to avoid stale pointers */
	    vga.draw.linear_base = vga.mem.linear;
        vga.tandy.draw_base = vga.mem.linear;
        vga.tandy.mem_base = vga.mem.linear;

        /* PC-98 pointers */
        pc98_pgraph_current_cpu_page = vga.mem.linear + PC98_VRAM_GRAPHICS_OFFSET;
        pc98_pgraph_current_display_page = vga.mem.linear + PC98_VRAM_GRAPHICS_OFFSET;

        /* parallel system */
        if (vga_alt_new_mode) {
            for (size_t si=0;si < VGA_Draw_2_elem;si++)
                vga.draw_2[si].draw_base = vga.mem.linear;

            vga.draw_2[0].horz.char_pixel_mask = 0xFFu;
            vga.draw_2[0].vert.char_pixel_mask = 0x1Fu;
        }

        /* may be related */
        VGA_SetupHandlers();
    }

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

// save state support
void *VGA_PageHandler_Func[16] =
{
	(void *) &vgaph.map,
	//(void *) &vgaph.changes,
	//(void *) &vgaph.text,
	(void *) &vgaph.tandy,
	//(void *) &vgaph.cega,
	//(void *) &vgaph.cvga,
	//(void *) &vgaph.uega,
	(void *) &vgaph.uvga,
	(void *) &vgaph.pcjr,
	(void *) &vgaph.herc,
	//(void *) &vgaph.lin4,
	(void *) &vgaph.lfb,
	//(void *) &vgaph.lfbchanges,
	(void *) &vgaph.mmio,
	(void *) &vgaph.empty,
};

void POD_Save_VGA_Memory( std::ostream& stream )
{
	// - static ptrs
	//uint8_t* linear;
	//uint8_t* linear_orgptr;


	// - pure data
	WRITE_POD_SIZE( vga.mem.linear, sizeof(uint8_t) * vga.mem.memsize);

	//***************************************************
	//***************************************************

	// static globals

	// - pure struct data
	WRITE_POD( &vgapages, vgapages );

	// - static classes
	//WRITE_POD( &vgaph, vgaph );
}


void POD_Load_VGA_Memory( std::istream& stream )
{
	// - static ptrs
	//uint8_t* linear;
	//uint8_t* linear_orgptr;


	// - pure data
	READ_POD_SIZE( vga.mem.linear, sizeof(uint8_t) * vga.mem.memsize);

	//***************************************************
	//***************************************************

	// static globals

	// - pure struct data
	READ_POD( &vgapages, vgapages );

	// - static classes
	//READ_POD( &vgaph, vgaph );
}
