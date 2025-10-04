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

#include <cassert>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dosbox.h"
#include "logging.h"
#include "mem.h"
#include "vga.h"
#include "paging.h"
#include "pic.h"
#include "inout.h"
#include "setup.h"
#include "cpu.h"
#include "control.h"
#include "pc98_cg.h"
#include "pc98_gdc.h"
#include "zipfile.h"
#include "src/ints/int10.h"

unsigned char pc98_pegc_mmio[0x200] = {0}; /* PC-98 memory-mapped PEGC registers at E0000h */
uint32_t pc98_pegc_banks[2] = {0x0000,0x0000}; /* bank switching offsets */
bool enveten = false, TTF_using(void);

extern bool non_cga_ignore_oddeven;
extern bool non_cga_ignore_oddeven_engage;
extern bool vga_fill_inactive_ram;
extern bool vga_ignore_extended_memory_bit;
extern bool memio_complexity_optimization;
extern bool enable_pc98_256color_planar;
extern bool enable_pc98_256color;
extern bool isa_memory_hole_15mb;

extern unsigned int vbe_window_granularity;
extern unsigned int vbe_window_size;
extern const char* RunningProgram;

uint32_t tandy_128kbase = 0x80000;

#define TANDY_VIDBASE(_X_)  &MemBase[ tandy_128kbase + (_X_)]

/* how much delay to add to VGA memory I/O in nanoseconds */
int vga_memio_delay_ns = 1000;
bool vga_memio_lfb_delay = false;

void VGAMEM_USEC_read_delay() {
	if (vga_memio_delay_ns > 0) {
		Bits delaycyc = (CPU_CycleMax * vga_memio_delay_ns) / 1000000;
//		if(GCC_UNLIKELY(CPU_Cycles < 3*delaycyc)) delaycyc = 0; //Else port access will set cycles to 0. which might trigger problem with games which read 16 bit values
		CPU_Cycles -= delaycyc;
		CPU_IODelayRemoved += delaycyc;
	}
}

void VGAMEM_USEC_write_delay() {
	if (vga_memio_delay_ns > 0) {
		Bits delaycyc = (CPU_CycleMax * vga_memio_delay_ns * 3) / (1000000 * 4);
//		if(GCC_UNLIKELY(CPU_Cycles < 3*delaycyc)) delaycyc = 0; //Else port access will set cycles to 0. which might trigger problem with games which read 16 bit values
		CPU_Cycles -= delaycyc;
		CPU_IODelayRemoved += delaycyc;
	}
}

template <class baseLFBHandler> class VGA_SlowLFBHandler : public baseLFBHandler {
	public:
		VGA_SlowLFBHandler() : baseLFBHandler(PFLAG_NOCODE) {}
		void writeb(PhysPt addr,uint8_t val) override {
			VGAMEM_USEC_write_delay();
			PageHandler_HostPtWriteB(this,addr,val);
		}
		void writew(PhysPt addr,uint16_t val) override {
			VGAMEM_USEC_write_delay();
			PageHandler_HostPtWriteW(this,addr,val);
		}
		void writed(PhysPt addr,uint32_t val) override {
			VGAMEM_USEC_write_delay();
			PageHandler_HostPtWriteD(this,addr,val);
		}

		uint8_t readb(PhysPt addr) override {
			VGAMEM_USEC_read_delay();
			return PageHandler_HostPtReadB(this,addr);
		}
		uint16_t readw(PhysPt addr) override {
			VGAMEM_USEC_read_delay();
			return PageHandler_HostPtReadW(this,addr);
		}
		uint32_t readd(PhysPt addr) override {
			VGAMEM_USEC_read_delay();
			return PageHandler_HostPtReadD(this,addr);
		}
};

template <class Size>
static INLINE void hostWrite(HostPt off, Bitu val) {
	if ( sizeof( Size ) == 1)
		host_writeb( off, (uint8_t)val );
	else if ( sizeof( Size ) == 2)
		host_writew( off, (uint16_t)val );
	else if ( sizeof( Size ) == 4)
		host_writed( off, (uint32_t)val );
}

template <class Size>
static INLINE Bitu hostRead(HostPt off ) {
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
INLINE static uint32_t RasterOp(uint32_t input,uint32_t mask) {
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

INLINE static uint32_t ModeOperation(uint8_t val) {
	uint32_t full;
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

	vga.latch.d=((uint32_t*)vga.mem.linear)[planeaddr];
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
	uint32_t mask = vga.config.full_map_mask;

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

	uint32_t data=ModeOperation(val);
	VGA_Latch pixels;

	pixels.d =((uint32_t*)vga.mem.linear)[planeaddr];
	pixels.d&=~mask;
	pixels.d|=(data & mask);

	((uint32_t*)vga.mem.linear)[planeaddr]=pixels.d;
}

// Fast version especially for 256-color mode.
// In most cases all the remapping, bit operations, and such are unnecessary, and having an alternate
// path for this case hopefully addresses complaints by other DOSBox forks and users about "worse VGA
// performance"
class VGA_ChainedVGA_Handler : public PageHandler {
public:
	VGA_ChainedVGA_Handler() : PageHandler(PFLAG_NOCODE) {}
	template <typename T> static INLINE bool withinplanes(const PhysPt a) {
		/* NTS: Comparing against template parameter is optimized down to the line of code matching size of type T with no runtime compare */
		if (sizeof(T) == 4) return (a & 3u) == 0;
		if (sizeof(T) == 2) return (a & 1u) == 0;
		return true;
	}
	static INLINE PhysPt chain4remap(const PhysPt addr) {
		/* This is how chained 256-color mode actually works when mapping to planar memory underneath.
		 * This is why 256-color mode needs the DWORD mode of the CRTC and why switching to BYTE mode
		 * messes up the display (except on Tseng Labs ET3000/ET4000 cards). */
		// planar byte offset = addr & ~3u      (discard low 2 bits)
		// planer index = addr & 3u             (use low 2 bits as plane index)
		return ((addr & ~3u) << 2u) + (addr & 3u);
	}
	static INLINE PhysPt map(const PhysPt addr) {
		return chain4remap((PAGING_GetPhysicalAddress(addr)&vgapages.mask)+(PhysPt)vga.svga.bank_read_full)&vga.mem.memmask;
	}
	template <typename T=uint8_t> static INLINE T do_read_aligned(const PhysPt a) {
		return *((T*)(&vga.mem.linear[a]));
	}
	template <typename T=uint8_t> static INLINE T do_read(const PhysPt a) {
		if (withinplanes<T>(a)) /* aligned, do a fast typecast read */
			return do_read_aligned<T>(map(a));
		else if (sizeof(T) == 4) /* not aligned, split 32-bit to two 16-bit */
			return (uint32_t)do_read<uint16_t>(a) + ((uint32_t)do_read<uint16_t>(a+2) << (uint32_t)16u);
		else if (sizeof(T) == 2) /* not aligned, split 16-bit to two 8-bit */
			return (uint16_t)do_read<uint8_t>(a) + ((uint16_t)do_read<uint8_t>(a+1) << (uint32_t)8u);
		else
			return 0xFF; /* should not happen, byte I/O is always aligned */
	}
	template <typename T=uint8_t> static INLINE void do_write_aligned(const PhysPt a,const T v) {
		*((T*)(&vga.mem.linear[a])) = v;
	}
	template <typename T=uint8_t> static INLINE void do_write(const PhysPt a,const T v) {
		if (withinplanes<T>(a)) /* aligned, do a fast typecast write */
			do_write_aligned<T>(map(a),v);
		else if (sizeof(T) == 4) /* not aligned, split 32-bit to two 16-bit */
			{ do_write<uint16_t>(a,uint16_t(v)); do_write<uint16_t>(a+2,uint16_t(v >> (T)16u)); }
		else if (sizeof(T) == 2) /* not aligned, split 16-bit to two 8-bit */
			{ do_write<uint8_t>(a,uint8_t(v)); do_write<uint8_t>(a+1,uint8_t(v >> (T)8u)); }
	}

	uint8_t readb(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint8_t>(addr);
	}
	uint16_t readw(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint16_t>(addr);
	}
	uint32_t readd(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint32_t>(addr);
	}
	void writeb(PhysPt addr, uint8_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint8_t>(addr,val);
	}
	void writew(PhysPt addr,uint16_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint16_t>(addr,val);
	}
	void writed(PhysPt addr,uint32_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint32_t>(addr,val);
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
	static INLINE PhysPt map(const PhysPt addr) {
		return (PAGING_GetPhysicalAddress(addr)&vgapages.mask)+(PhysPt)vga.svga.bank_read_full; /* masking with vga.mem.memmask not necessary */
	}

	// planar byte offset = addr & ~3u      (discard low 2 bits)
	// planer index = addr & 3u             (use low 2 bits as plane index)
	static INLINE uint8_t readHandler8(PhysPt addr) {
		// FIXME: Does chained mode use the lower 2 bits of the CPU address or does it use the read mode select???
		return VGA_Generic_Read_Handler(addr&~3u, addr, (uint8_t)(addr&3u));
	}
	static INLINE void writeHandler8(PhysPt addr, uint8_t val) {
		return VGA_Generic_Write_Handler<true/*chained*/>(addr&~3u, addr, (uint8_t)val);
	}

	/* Your C++ compiler can probably optimize this down to just the optimal calls to based on type T... at least GCC does! */
	template <typename T=uint8_t> static INLINE T do_read(const PhysPt a) {
		if (sizeof(T) == 4) /* 32-bit to two 16-bit */
			return (uint32_t)do_read<uint16_t>(a) + ((uint32_t)do_read<uint16_t>(a+2) << (uint32_t)16u);
		else if (sizeof(T) == 2) /* 16-bit to two 8-bit */
			return (uint16_t)do_read<uint8_t>(a) + ((uint16_t)do_read<uint8_t>(a+1) << (uint32_t)8u);
		else
			return (T)readHandler8(map(a));
	}
	template <typename T=uint8_t> static INLINE void do_write(const PhysPt a,const T v) {
		if (sizeof(T) == 4) /* split 32-bit to two 16-bit */
			{ do_write<uint16_t>(a,uint16_t(v)); do_write<uint16_t>(a+2,uint16_t(v >> (T)16u)); }
		else if (sizeof(T) == 2) /* split 16-bit to two 8-bit */
			{ do_write<uint8_t>(a,uint8_t(v)); do_write<uint8_t>(a+1,uint8_t(v >> (T)8u)); }
		else
			writeHandler8(map(a),v);
	}

	uint8_t readb(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint8_t>(addr);
	}
	uint16_t readw(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint16_t>(addr);
	}
	uint32_t readd(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint32_t>(addr);
	}
	void writeb(PhysPt addr, uint8_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint8_t>(addr,val);
	}
	void writew(PhysPt addr,uint16_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint16_t>(addr,val);
	}
	void writed(PhysPt addr,uint32_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint32_t>(addr,val);
	}
};

class VGA_ET4000_ChainedVGA_Slow_Handler : public PageHandler {
public:
	VGA_ET4000_ChainedVGA_Slow_Handler() : PageHandler(PFLAG_NOCODE) {}
	static INLINE PhysPt map(const PhysPt addr) {
		return (PAGING_GetPhysicalAddress(addr)&vgapages.mask)+(PhysPt)vga.svga.bank_read_full; /* masking with vga.mem.memmask not necessary */
	}

	// planar byte offset = addr >> 2       (shift 2 bits to the right)
	// planer index = addr & 3u             (use low 2 bits as plane index)
	static INLINE uint8_t readHandler8(PhysPt addr) {
		// FIXME: Does chained mode use the lower 2 bits of the CPU address or does it use the read mode select???
		return VGA_Generic_Read_Handler(addr>>2u, addr, (uint8_t)(addr&3u));
	}
	static INLINE void writeHandler8(PhysPt addr, uint8_t val) {
		return VGA_Generic_Write_Handler<true/*chained*/>(addr>>2u, addr, (uint8_t)val);
	}

	/* Your C++ compiler can probably optimize this down to just the optimal calls to based on type T... at least GCC does! */
	template <typename T=uint8_t> static INLINE T do_read(const PhysPt a) {
		if (sizeof(T) == 4) /* 32-bit to two 16-bit */
			return (uint32_t)do_read<uint16_t>(a) + ((uint32_t)do_read<uint16_t>(a+2) << (uint32_t)16u);
		else if (sizeof(T) == 2) /* 16-bit to two 8-bit */
			return (uint16_t)do_read<uint8_t>(a) + ((uint16_t)do_read<uint8_t>(a+1) << (uint32_t)8u);
		else
			return (T)readHandler8(map(a));
	}
	template <typename T=uint8_t> static INLINE void do_write(const PhysPt a,const T v) {
		if (sizeof(T) == 4) /* split 32-bit to two 16-bit */
			{ do_write<uint16_t>(a,uint16_t(v)); do_write<uint16_t>(a+2,uint16_t(v >> (T)16u)); }
		else if (sizeof(T) == 2) /* split 16-bit to two 8-bit */
			{ do_write<uint8_t>(a,uint8_t(v)); do_write<uint8_t>(a+1,uint8_t(v >> (T)8u)); }
		else
			writeHandler8(map(a),v);
	}

	uint8_t readb(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint8_t>(addr);
	}
	uint16_t readw(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint16_t>(addr);
	}
	uint32_t readd(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint32_t>(addr);
	}
	void writeb(PhysPt addr, uint8_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint8_t>(addr,val);
	}
	void writew(PhysPt addr,uint16_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint16_t>(addr,val);
	}
	void writed(PhysPt addr,uint32_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint32_t>(addr,val);
	}
};

class VGA_UnchainedVGA_Handler : public PageHandler {
public:
	VGA_UnchainedVGA_Handler() : PageHandler(PFLAG_NOCODE) {}
	static INLINE PhysPt map(const PhysPt addr) {
		return (PAGING_GetPhysicalAddress(addr)&vgapages.mask)+(PhysPt)vga.svga.bank_read_full; /* masking with vga.mem.memmask>>2 not necessary */
	}

	static INLINE uint8_t readHandler8(PhysPt addr) {
		return VGA_Generic_Read_Handler(addr, addr, vga.config.read_map_select);
	}
	static INLINE void writeHandler8(PhysPt addr, uint8_t val) {
		VGA_Generic_Write_Handler<false/*chained*/>(addr, addr, val);
	}

	/* Your C++ compiler can probably optimize this down to just the optimal calls to based on type T... at least GCC does! */
	template <typename T=uint8_t> static INLINE T do_read(const PhysPt a) {
		if (sizeof(T) == 4) /* 32-bit to two 16-bit */
			return (uint32_t)do_read<uint16_t>(a) + ((uint32_t)do_read<uint16_t>(a+2) << (uint32_t)16u);
		else if (sizeof(T) == 2) /* 16-bit to two 8-bit */
			return (uint16_t)do_read<uint8_t>(a) + ((uint16_t)do_read<uint8_t>(a+1) << (uint32_t)8u);
		else
			return (T)readHandler8(map(a));
	}
	template <typename T=uint8_t> static INLINE void do_write(const PhysPt a,const T v) {
		if (sizeof(T) == 4) /* split 32-bit to two 16-bit */
			{ do_write<uint16_t>(a,uint16_t(v)); do_write<uint16_t>(a+2,uint16_t(v >> (T)16u)); }
		else if (sizeof(T) == 2) /* split 16-bit to two 8-bit */
			{ do_write<uint8_t>(a,uint8_t(v)); do_write<uint8_t>(a+1,uint8_t(v >> (T)8u)); }
		else
			writeHandler8(map(a),v);
	}

	uint8_t readb(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint8_t>(addr);
	}
	uint16_t readw(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint16_t>(addr);
	}
	uint32_t readd(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint32_t>(addr);
	}
	void writeb(PhysPt addr, uint8_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint8_t>(addr,val);
	}
	void writew(PhysPt addr,uint16_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint16_t>(addr,val);
	}
	void writed(PhysPt addr,uint32_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint32_t>(addr,val);
	}
};

// This version assumes that no raster ops, bit shifts, bit masking, or complicated stuff is enabled
// so that DOS games using 256-color unchained mode in a simple way, or games with simple handling
// of 16-color planar modes, can see a performance improvement. Reading still uses the slower generic
// code because it's uncommon to read back and the unchained mode has planar actions to it.
class VGA_UnchainedVGA_Fast_Handler : public VGA_UnchainedVGA_Handler {
public:
	VGA_UnchainedVGA_Fast_Handler() : VGA_UnchainedVGA_Handler() {}

	/* must mask for writing because the write handler does array lookup */
	static INLINE PhysPt map(const PhysPt addr) {
		return ((PAGING_GetPhysicalAddress(addr)&vgapages.mask)+(PhysPt)vga.svga.bank_read_full)&(vga.mem.memmask>>2u);
	}

	static INLINE void writeHandler8(PhysPt addr, uint8_t val) {
		((uint32_t*)vga.mem.linear)[addr] =
			(((uint32_t*)vga.mem.linear)[addr] & vga.config.full_not_map_mask) + (ExpandTable[val] & vga.config.full_map_mask);
	}

	template <typename T=uint8_t> static INLINE void do_write(const PhysPt a,const T v) {
		if (sizeof(T) == 4) /* split 32-bit to two 16-bit */
			{ do_write<uint16_t>(a,uint16_t(v)); do_write<uint16_t>(a+2,uint16_t(v >> (T)16u)); }
		else if (sizeof(T) == 2) /* split 16-bit to two 8-bit */
			{ do_write<uint8_t>(a,uint8_t(v)); do_write<uint8_t>(a+1,uint8_t(v >> (T)8u)); }
		else
			writeHandler8(map(a),v);
	}

	void writeb(PhysPt addr, uint8_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint8_t>(addr,val);
	}
	void writew(PhysPt addr,uint16_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint16_t>(addr,val);
	}
	void writed(PhysPt addr,uint32_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint32_t>(addr,val);
	}
};

#include <stdio.h>

class VGA_CGATEXT_PageHandler : public PageHandler {
public:
	VGA_CGATEXT_PageHandler() {
		flags=PFLAG_NOCODE;
	}
	uint8_t readb(PhysPt addr) override {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr) & 0x3FFF;
		return vga.tandy.mem_base[addr];
	}
	void writeb(PhysPt addr,uint8_t val) override {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr) & 0x3FFF;
		vga.tandy.mem_base[addr] = val;

		if (enableCGASnow) {
			/* NTS: We can't use PIC_FullIndex() exclusively because it's not precise enough
			 *      with respect to when DOSBox CPU emulation is writing. We have to use other
			 *      variables like CPU_Cycles to gain additional precision */
			double timeInFrame = PIC_FullIndex()-vga.draw.delay.framestart;
			double timeInLine = fmod(timeInFrame,vga.draw.delay.htotal);

			/* we're in active area. which column should the snow show up on? */
			uint32_t x = (uint32_t)((timeInLine * 80) / vga.draw.delay.hblkstart);
			if ((unsigned)x < 80) vga.draw.cga_snow[x] = val;
		}
	}
};

class VGA_MCGATEXT_PageHandler : public PageHandler {
public:
	VGA_MCGATEXT_PageHandler() {
		flags=PFLAG_NOCODE;
	}
	uint8_t readb(PhysPt addr) override {
		addr = PAGING_GetPhysicalAddress(addr) & 0xFFFF;
		VGAMEM_USEC_read_delay();
		return vga.tandy.mem_base[addr];
	}
	void writeb(PhysPt addr,uint8_t val) override {
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

/* Generic EGC ROP handling according to Neko Project II.
 * I assume it uses specialized functions for common ROPs as an optimization method
 * since this generic handler does a lot of conditional bit operations. This is
 * ope_xx transcribed for DOSBox-X. */
static egc_quad &ope_xx(uint8_t ope, const PhysPt vramoff) {
	egc_quad pat;
	egc_quad dst;

	egc_fetch_planar<uint16_t>(/*&*/dst,vramoff);

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

		// TODO: NP2kai source code (Neko Project II KAI) suggests the illegal value 11b (3) returns one foreground and one background color.
		//       Ref: https://github.com/AZO234/NP2kai mem/memegc.c line 774 ope_nd and ope_xx. I don't know if any games rely on that, but
		//       it might improve emulation accuracy to support it.

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
		pc98_egc_data[0].w |= (pat[0].w & pc98_egc_src[0].w & dst[0].w);
		pc98_egc_data[1].w |= (pat[1].w & pc98_egc_src[1].w & dst[1].w);
		pc98_egc_data[2].w |= (pat[2].w & pc98_egc_src[2].w & dst[2].w);
		pc98_egc_data[3].w |= (pat[3].w & pc98_egc_src[3].w & dst[3].w);
	}
	if (ope & 0x40) {
		pc98_egc_data[0].w |= ((~pat[0].w) & pc98_egc_src[0].w & dst[0].w);
		pc98_egc_data[1].w |= ((~pat[1].w) & pc98_egc_src[1].w & dst[1].w);
		pc98_egc_data[2].w |= ((~pat[2].w) & pc98_egc_src[2].w & dst[2].w);
		pc98_egc_data[3].w |= ((~pat[3].w) & pc98_egc_src[3].w & dst[3].w);
	}
	if (ope & 0x20) {
		pc98_egc_data[0].w |= (pat[0].w & pc98_egc_src[0].w & (~dst[0].w));
		pc98_egc_data[1].w |= (pat[1].w & pc98_egc_src[1].w & (~dst[1].w));
		pc98_egc_data[2].w |= (pat[2].w & pc98_egc_src[2].w & (~dst[2].w));
		pc98_egc_data[3].w |= (pat[3].w & pc98_egc_src[3].w & (~dst[3].w));
	}
	if (ope & 0x10) {
		pc98_egc_data[0].w |= ((~pat[0].w) & pc98_egc_src[0].w & (~dst[0].w));
		pc98_egc_data[1].w |= ((~pat[1].w) & pc98_egc_src[1].w & (~dst[1].w));
		pc98_egc_data[2].w |= ((~pat[2].w) & pc98_egc_src[2].w & (~dst[2].w));
		pc98_egc_data[3].w |= ((~pat[3].w) & pc98_egc_src[3].w & (~dst[3].w));
	}
	if (ope & 0x08) {
		pc98_egc_data[0].w |= (pat[0].w & (~pc98_egc_src[0].w) & dst[0].w);
		pc98_egc_data[1].w |= (pat[1].w & (~pc98_egc_src[1].w) & dst[1].w);
		pc98_egc_data[2].w |= (pat[2].w & (~pc98_egc_src[2].w) & dst[2].w);
		pc98_egc_data[3].w |= (pat[3].w & (~pc98_egc_src[3].w) & dst[3].w);
	}
	if (ope & 0x04) {
		pc98_egc_data[0].w |= ((~pat[0].w) & (~pc98_egc_src[0].w) & dst[0].w);
		pc98_egc_data[1].w |= ((~pat[1].w) & (~pc98_egc_src[1].w) & dst[1].w);
		pc98_egc_data[2].w |= ((~pat[2].w) & (~pc98_egc_src[2].w) & dst[2].w);
		pc98_egc_data[3].w |= ((~pat[3].w) & (~pc98_egc_src[3].w) & dst[3].w);
	}
	if (ope & 0x02) {
		pc98_egc_data[0].w |= (pat[0].w & (~pc98_egc_src[0].w) & (~dst[0].w));
		pc98_egc_data[1].w |= (pat[1].w & (~pc98_egc_src[1].w) & (~dst[1].w));
		pc98_egc_data[2].w |= (pat[2].w & (~pc98_egc_src[2].w) & (~dst[2].w));
		pc98_egc_data[3].w |= (pat[3].w & (~pc98_egc_src[3].w) & (~dst[3].w));
	}
	if (ope & 0x01) {
		pc98_egc_data[0].w |= ((~pat[0].w) & (~pc98_egc_src[0].w) & (~dst[0].w));
		pc98_egc_data[1].w |= ((~pat[1].w) & (~pc98_egc_src[1].w) & (~dst[1].w));
		pc98_egc_data[2].w |= ((~pat[2].w) & (~pc98_egc_src[2].w) & (~dst[2].w));
		pc98_egc_data[3].w |= ((~pat[3].w) & (~pc98_egc_src[3].w) & (~dst[3].w));
	}

	return pc98_egc_data;
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

		// TODO: NP2kai source code (Neko Project II KAI) suggests the illegal value 11b (3) returns one foreground and one background color.
		//       Ref: https://github.com/AZO234/NP2kai mem/memegc.c line 774 ope_nd and ope_xx. I don't know if any games rely on that, but
		//       it might improve emulation accuracy to support it.

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

		// TODO: NP2kai source code (Neko Project II KAI) suggests the illegal value 11b (3) returns one foreground and one background color.
		//       Ref: https://github.com/AZO234/NP2kai mem/memegc.c line 774 ope_nd and ope_xx. I don't know if any games rely on that, but
		//       it might improve emulation accuracy to support it.

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
			// TODO: NP2kai source code (Neko Project II KAI) suggests the illegal value 11b (3) returns one foreground and one background color.
			//       Ref: https://github.com/AZO234/NP2kai mem/memegc.c line 774 ope_nd and ope_xx. I don't know if any games rely on that, but
			//       it might improve emulation accuracy to support it.

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
		uint8_t readb(PhysPt addr) override {
			addr = PAGING_GetPhysicalAddress(addr) & 0x3FFFu;

			if (addr >= 0x3FE0u)
				return pc98_mem_msw((addr >> 2u) & 7u);
			else if ((addr & 0x2001u) == 0x2001u)
				return (uint8_t)(~0u); /* Odd bytes of attribute RAM do not exist, apparently */

			return VRAM98_TEXT[addr];
		}
		void writeb(PhysPt addr,uint8_t val) override {
			addr = PAGING_GetPhysicalAddress(addr) & 0x3FFFu;

			if (addr >= 0x3FE0u)
				return pc98_mem_msw_write((addr >> 2u) & 7u,(unsigned char)val);
			else if ((addr & 0x2001u) == 0x2001u)
				return;             /* Odd bytes of attribute RAM do not exist, apparently */

			VRAM98_TEXT[addr] = (unsigned char)val;
		}
};

extern uint16_t a1_font_load_addr;
extern uint8_t a1_font_char_offset;

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
		uint8_t readb(PhysPt addr) override {
			/* uses the low 12 bits and therefore does not need PAGING_GetPhysicalAddress() */
			uint8_t high = a1_font_load_addr & 0xff;
			if ((high >= 0x09 && high <= 0x0b) || (high >= 0x0c && high <= 0x0f) || (high >= 0x56 && high <= 0x5f)) {
				if(addr & 1)
					return pc98_font_char_read(a1_font_load_addr,(addr >> 1) & 0xF, (a1_font_char_offset & 0x20) ? 0 : 1);
				else
					return 0;
			}

			return pc98_font_char_read(a1_font_load_addr,(addr >> 1) & 0xF,addr & 1);
		}
		void writeb(PhysPt addr,uint8_t val) override {
			/* uses the low 12 bits and therefore does not need PAGING_GetPhysicalAddress() */
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
		uint8_t readb(PhysPt addr) override {
			return pc98_pegc_mmio_read(PAGING_GetPhysicalAddress(addr) & 0x7FFFu);
		}
		void writeb(PhysPt addr,uint8_t val) override {
			pc98_pegc_mmio_write(PAGING_GetPhysicalAddress(addr) & 0x7FFFu,val);
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
		uint8_t readb(PhysPt addr) override {
			(void)addr;

			// LOG_MSG("PEGC 256-color planar warning: Readb from %lxh",(unsigned long)addr);
			return (uint8_t)(~0);
		}
		void writeb(PhysPt addr,uint8_t val) override {
			(void)addr;
			(void)val;

			// LOG_MSG("PEGC 256-color planar warning: Writeb to %lxh val %02xh",(unsigned long)addr,(unsigned int)val);
		}
		uint16_t readw(PhysPt addr) override {
			(void)addr;

			// LOG_MSG("PEGC 256-color planar warning: Readw from %lxh",(unsigned long)addr);
			return (uint16_t)(~0);
		}
		void writew(PhysPt addr,uint16_t val) override {
			(void)addr;
			(void)val;

			// LOG_MSG("PEGC 256-color planar warning: Writew to %lxh val %04xh",(unsigned long)addr,(unsigned int)val);
		}
};

// A8000h is bank 0
// B0000h is bank 1
template <const unsigned int bank> class VGA_PC98_256BANK_PageHandler : public PageHandler {
	public:
		VGA_PC98_256BANK_PageHandler() : PageHandler(PFLAG_NOCODE) {}
		uint8_t readb(PhysPt addr) override {
			return pc98_vram_256bank_from_window(bank)[PAGING_GetPhysicalAddress(addr) & 0x7FFFu];
		}
		void writeb(PhysPt addr,uint8_t val) override {
			pc98_vram_256bank_from_window(bank)[PAGING_GetPhysicalAddress(addr) & 0x7FFFu] = val;
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
				return *((AWT*)(pc98_egc_src[pc98_egc_lead_plane&3].b+(vramoff&1)));
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
	uint8_t readb(PhysPt addr) override {
		VGAMEM_USEC_read_delay();
		return readc<uint8_t>( PAGING_GetPhysicalAddress(addr) );
	}
	void writeb(PhysPt addr,uint8_t val) override {
		VGAMEM_USEC_write_delay();
		writec<uint8_t>( PAGING_GetPhysicalAddress(addr), val );
	}

	/* word-wise.
	 * in the style of the 8086, non-word-aligned I/O is split into byte I/O */
	uint16_t readw(PhysPt addr) override {
		VGAMEM_USEC_read_delay();
		addr = PAGING_GetPhysicalAddress(addr);
		if (!(addr & 1)) /* if WORD aligned */
			return readc<uint16_t>(addr);
		else {
			return   (unsigned int)readc<uint8_t>(addr+0U) +
				((unsigned int)readc<uint8_t>(addr+1U) << 8u);
		}
	}
	void writew(PhysPt addr,uint16_t val) override {
		VGAMEM_USEC_write_delay();
		addr = PAGING_GetPhysicalAddress(addr);
		if (!(addr & 1)) /* if WORD aligned */
			writec<uint16_t>(addr,val);
		else {
			writec<uint8_t>(addr+0,(uint8_t)val);
			writec<uint8_t>(addr+1,(uint8_t)(val >> 8U));
		}
	}
};

class VGA_PC98_LFB_Handler : public PageHandler { // with slow adapter
public:
	VGA_PC98_LFB_Handler() : PageHandler(PFLAG_READABLE|PFLAG_WRITEABLE|PFLAG_NOCODE) {}
	VGA_PC98_LFB_Handler(const unsigned int fl) : PageHandler(fl) {}
	HostPt GetHostReadPt(PageNum phys_page) override {
		return &vga.mem.linear[(phys_page&0x7F)*4096 + PC98_VRAM_GRAPHICS_OFFSET]; /* 512KB mapping */
	}
	HostPt GetHostWritePt(PageNum phys_page) override {
		return &vga.mem.linear[(phys_page&0x7F)*4096 + PC98_VRAM_GRAPHICS_OFFSET]; /* 512KB mapping */
	}
};

class VGA_Map_Handler : public PageHandler { // with slow adapter
public:
	VGA_Map_Handler() : PageHandler(PFLAG_READABLE|PFLAG_WRITEABLE|PFLAG_NOCODE) {}
	VGA_Map_Handler(const unsigned int fl) : PageHandler(fl) {}
	HostPt GetHostReadPt(PageNum phys_page) override {
		phys_page-=vgapages.base;
		return &vga.mem.linear[(vga.svga.bank_read_full+phys_page*4096)&vga.mem.memmask];
	}
	HostPt GetHostWritePt(PageNum phys_page) override {
		phys_page-=vgapages.base;
		return &vga.mem.linear[(vga.svga.bank_write_full+phys_page*4096)&vga.mem.memmask];
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

	uint8_t readb(PhysPt addr) override {
		delay();
		return vga.tandy.mem_base[(PAGING_GetPhysicalAddress(addr) - 0xb8000) & 0x3FFF];
	}
	void writeb(PhysPt addr,uint8_t val) override {
		delay();
		vga.tandy.mem_base[(PAGING_GetPhysicalAddress(addr) - 0xb8000) & 0x3FFF] = val;
	}
	
};

class VGA_LFB_Handler : public PageHandler { // with slow adapter
public:
	VGA_LFB_Handler() : PageHandler(PFLAG_READABLE|PFLAG_WRITEABLE|PFLAG_NOCODE) {}
	VGA_LFB_Handler(const unsigned int fl) : PageHandler(fl) {}
	HostPt GetHostReadPt( PageNum phys_page ) override {
		phys_page -= vga.lfb.page;
		phys_page &= vga.mem.memmask >> 12u;
		return &vga.mem.linear[(phys_page*4096)&vga.mem.memmask];
	}
	HostPt GetHostWritePt( PageNum phys_page ) override {
		return GetHostReadPt( phys_page );
	}
};

extern void XGA_Write(Bitu port, Bitu val, Bitu len);
extern Bitu XGA_Read(Bitu port, Bitu len);

class VGA_MMIO_Handler : public PageHandler {
public:
	VGA_MMIO_Handler() : PageHandler(PFLAG_NOCODE) {}
	void writeb(PhysPt addr,uint8_t val) override {
		VGAMEM_USEC_write_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		XGA_Write(port, val, 1);
	}
	void writew(PhysPt addr,uint16_t val) override {
		VGAMEM_USEC_write_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		XGA_Write(port, val, 2);
	}
	void writed(PhysPt addr,uint32_t val) override {
		VGAMEM_USEC_write_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		XGA_Write(port, val, 4);
	}

	uint8_t readb(PhysPt addr) override {
		VGAMEM_USEC_read_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		return (uint8_t)XGA_Read(port, 1);
	}
	uint16_t readw(PhysPt addr) override {
		VGAMEM_USEC_read_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		return (uint16_t)XGA_Read(port, 2);
	}
	uint32_t readd(PhysPt addr) override {
		VGAMEM_USEC_read_delay();
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
		return (uint32_t)XGA_Read(port, 4);
	}
};

class VGA_TANDY_PageHandler : public PageHandler { // with slow adapter
public:
	VGA_TANDY_PageHandler() : PageHandler(PFLAG_READABLE|PFLAG_WRITEABLE) {}
	VGA_TANDY_PageHandler(const unsigned int fl) : PageHandler(fl) {}
	HostPt GetHostReadPt(PageNum phys_page) override {
		// Odd banks are limited to 16kB and repeated
		if (vga.tandy.mem_bank & 1) 
			phys_page&=0x03;
		else 
			phys_page&=0x07;
		return vga.tandy.mem_base + (phys_page * 4096);
	}
	HostPt GetHostWritePt(PageNum phys_page) override {
		return GetHostReadPt( phys_page );
	}
};


class VGA_PCJR_Handler : public PageHandler { // with slow adapter
public:
	VGA_PCJR_Handler() : PageHandler(PFLAG_READABLE|PFLAG_WRITEABLE) {}
	VGA_PCJR_Handler(const unsigned int fl) : PageHandler(fl) {}
	HostPt GetHostReadPt(PageNum phys_page) override {
		phys_page-=0xb8;
		// The 16kB map area is repeated in the 32kB range
		// On CGA CPU A14 is not decoded so it repeats there too
		phys_page&=0x03;
		return vga.tandy.mem_base + (phys_page * 4096);
	}
	HostPt GetHostWritePt(PageNum phys_page) override {
		return GetHostReadPt( phys_page );
	}
};

class VGA_AMS_Handler : public PageHandler {
public:
	VGA_AMS_Handler() : PageHandler(PFLAG_NOCODE) {}
	VGA_AMS_Handler(const unsigned int fl) : PageHandler(fl) {}
	static INLINE PhysPt map(PhysPt addr) {
		if (vga.mode == M_AMSTRAD) {
			return PAGING_GetPhysicalAddress(addr) & 0x3fffu;
		}
		else {
			addr = PAGING_GetPhysicalAddress(addr) & 0x7fffu;
			PhysPt phys_page = addr >> 12;
			//test for a unaligned bank, then replicate 2x16kb
			if (vga.tandy.mem_bank & 1) phys_page &= 0x03;
			return (phys_page * 4096) + (addr & 0xfff);
		}
	}
	static INLINE PhysPt mapread(const PhysPt addr) {
		return map(addr) + (vga.amstrad.read_plane * 0x4000u);
	}
	template <typename T=uint8_t> static INLINE T do_read_aligned(const PhysPt a) {
		return *((T*)(&vga.tandy.mem_base[a]));
	}
	template <typename T=uint8_t> static INLINE T do_read(const PhysPt a) {
		if (sizeof(T) == 4) /* not aligned, split 32-bit to two 16-bit */
			return (uint32_t)do_read<uint16_t>(a) + ((uint32_t)do_read<uint16_t>(a+2) << (uint32_t)16u);
		else if (sizeof(T) == 2) /* not aligned, split 16-bit to two 8-bit */
			return (uint16_t)do_read<uint8_t>(a) + ((uint16_t)do_read<uint8_t>(a+1) << (uint32_t)8u);
		else
			return (T)do_read_aligned(mapread(a));
	}
	template <typename T=uint8_t> static INLINE void do_write_aligned(const PhysPt a,const T v) {
		const uint8_t plane = (vga.mode==M_AMSTRAD) ? vga.amstrad.write_plane : 0x01; // 0x0F?
		if (plane & 0x08) *((T*)(&vga.tandy.mem_base[a+0xC000u])) = v;
		if (plane & 0x04) *((T*)(&vga.tandy.mem_base[a+0x8000u])) = v;
		if (plane & 0x02) *((T*)(&vga.tandy.mem_base[a+0x4000u])) = v;
		if (plane & 0x01) *((T*)(&vga.tandy.mem_base[a        ])) = v;
	}
	template <typename T=uint8_t> static INLINE void do_write(const PhysPt a,const T v) {
		if (sizeof(T) == 4) /* not aligned, split 32-bit to two 16-bit */
			{ do_write<uint16_t>(a,uint16_t(v)); do_write<uint16_t>(a+2,uint16_t(v >> (T)16u)); }
		else if (sizeof(T) == 2) /* not aligned, split 16-bit to two 8-bit */
			{ do_write<uint8_t>(a,uint8_t(v)); do_write<uint8_t>(a+1,uint8_t(v >> (T)8u)); }
		else
			do_write_aligned(map(a),v);
	}

	uint8_t readb(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint8_t>(addr);
	}
	uint16_t readw(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint16_t>(addr);
	}
	uint32_t readd(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint32_t>(addr);
	}
	void writeb(PhysPt addr, uint8_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint8_t>(addr,val);
	}
	void writew(PhysPt addr,uint16_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint16_t>(addr,val);
	}
	void writed(PhysPt addr,uint32_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint32_t>(addr,val);
	}
};

class VGA_HERC_Handler : public PageHandler { // with slow adapter
public:
	VGA_HERC_Handler() : PageHandler(PFLAG_READABLE|PFLAG_WRITEABLE) {}
	VGA_HERC_Handler(const unsigned int fl) : PageHandler(fl) {}
	HostPt GetHostReadPt(PageNum phys_page) override {
		(void)phys_page;//UNUSED
		// The 4kB map area is repeated in the 32kB range
		return &vga.mem.linear[0];
	}
	HostPt GetHostWritePt(PageNum phys_page) override {
		return GetHostReadPt( phys_page );
	}
};

class VGA_Empty_Handler : public PageHandler {
public:
	VGA_Empty_Handler() : PageHandler(PFLAG_NOCODE) {}
	uint8_t readb(PhysPt /*addr*/) override {
//		LOG(LOG_VGA, LOG_NORMAL ) ( "Read from empty memory space at %x", addr );
		return 0xff;
	} 
	void writeb(PhysPt /*addr*/,uint8_t /*val*/) override {
//		LOG(LOG_VGA, LOG_NORMAL ) ( "Write %x to empty memory space at %x", val, addr );
	}
};

void InColorLoadLatch(const VGA_Latch &latch) {
	const uint32_t nochangemask = vga.herc.latchprotect * 0x01010101u; /* i.e. 0x5A -> 0x5A5A5A5A */
	vga.herc.latch = (vga.herc.latch & nochangemask) + (latch.d & (~nochangemask));
}

/* Text region of the InColor which is documented to write to all bitplanes bypassing the masking functions entirely */
/* NTS: If the InColor is documented as writing all bitplanes unconditionally in monochrome backwards compatible mode,
 *      then it stands to reason that it doesn't have the Odd/Even mode EGA/VGA does and the text and attribute bytes
 *      remain interleaved even as four copies across all four bitplanes. Am I right? */
class HERC_InColor_Mono_Handler : public PageHandler {
public:
	HERC_InColor_Mono_Handler() : PageHandler(PFLAG_NOCODE) {}
	static INLINE PhysPt map(const PhysPt addr) {
		return PAGING_GetPhysicalAddress(addr)&vgapages.mask;
	}

	static INLINE uint8_t readHandler(PhysPt start) {
		/* TODO: Load hardware latch? */
		/* TODO: Which bitplane does it read? */
		VGA_Latch latch;
		latch.d = ((uint32_t*)vga.mem.linear)[start];
		InColorLoadLatch(latch);
		return latch.b[0];
	}
	static INLINE void writeHandler(PhysPt start, uint8_t val) {
		((uint32_t*)vga.mem.linear)[start] = ExpandTable[val];
	}

	/* Your C++ compiler can probably optimize this down to just the optimal calls to based on type T... at least GCC does! */
	template <typename T=uint8_t> static INLINE T do_read(const PhysPt a) {
		if (sizeof(T) == 4) /* 32-bit to two 16-bit */
			return (uint32_t)do_read<uint16_t>(a) + ((uint32_t)do_read<uint16_t>(a+2) << (uint32_t)16u);
		else if (sizeof(T) == 2) /* 16-bit to two 8-bit */
			return (uint16_t)do_read<uint8_t>(a) + ((uint16_t)do_read<uint8_t>(a+1) << (uint32_t)8u);
		else
			return (T)readHandler(map(a));
	}
	template <typename T=uint8_t> static INLINE void do_write(const PhysPt a,const T v) {
		if (sizeof(T) == 4) /* split 32-bit to two 16-bit */
			{ do_write<uint16_t>(a,uint16_t(v)); do_write<uint16_t>(a+2,uint16_t(v >> (T)16u)); }
		else if (sizeof(T) == 2) /* split 16-bit to two 8-bit */
			{ do_write<uint8_t>(a,uint8_t(v)); do_write<uint8_t>(a+1,uint8_t(v >> (T)8u)); }
		else
			writeHandler(map(a),v);
	}

	uint8_t readb(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint8_t>(addr);
	}
	uint16_t readw(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint16_t>(addr);
	}
	uint32_t readd(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint32_t>(addr);
	}
	void writeb(PhysPt addr, uint8_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint8_t>(addr,val);
	}
	void writew(PhysPt addr,uint16_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint16_t>(addr,val);
	}
	void writed(PhysPt addr,uint32_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint32_t>(addr,val);
	}
};

/* Graphics region of the InColor */
class HERC_InColor_Graphics_Handler : public PageHandler {
public:
	HERC_InColor_Graphics_Handler() : PageHandler(PFLAG_NOCODE) {}
	static INLINE PhysPt map(const PhysPt addr) {
		return PAGING_GetPhysicalAddress(addr)&vgapages.mask;
	}

	static uint8_t readHandler(PhysPt start) {
		const uint8_t idcmask = vga.herc.dont_care ^ 0xFu;
		uint32_t t1,t2,tmp;
		uint8_t result = 0;
		VGA_Latch latch;

		/* In the PC & PS/2 Video Systems book, there is a helpful diagram of how
		 * a read on Hercules InColor cards works. It suggests that the card loads
		 * the latch from video memory, excluding changes to the bits according
		 * to the Latch Protect, and then the color compare is made against the
		 * latch, not the raw data read from video memory.
		 *
		 * NTS: The diagram says that the dont care mask is ORed against both pixel
		 *      values and compared. We AND the values. Same difference, I think. */
		latch.d = ((uint32_t*)vga.mem.linear)[start];
		InColorLoadLatch(latch);

		t1 = t2 = vga.herc.latch;
		t1 = (t1 >> 4) & 0x0f0f0f0f;
		t2 &= 0x0f0f0f0f;

		tmp =   Expand16Table[0][(t1>>0)&0xFF] |
			Expand16Table[1][(t1>>8)&0xFF] |
			Expand16Table[2][(t1>>16)&0xFF] |
			Expand16Table[3][(t1>>24)&0xFF];

		for (unsigned int c=0;c < 4;c++)
			{ result |= (((tmp&idcmask) == (vga.herc.bgcolor&idcmask))?1u:0u) << (7-c); tmp >>= 8u; }

		tmp =   Expand16Table[0][(t2>>0)&0xFF] |
			Expand16Table[1][(t2>>8)&0xFF] |
			Expand16Table[2][(t2>>16)&0xFF] |
			Expand16Table[3][(t2>>24)&0xFF];

		for (unsigned int c=0;c < 4;c++)
			{ result |= (((tmp&idcmask) == (vga.herc.bgcolor&idcmask))?1u:0u) << (3-c); tmp >>= 8u; }

		return result ^ vga.herc.maskpolarity;
	}
	static void writeHandler(PhysPt start, uint8_t val) {
		const uint32_t valmask = ExpandTable[val];
		const uint32_t nochangemask = FillTable[vga.herc.planemask_protect];
		const uint32_t bgplane = FillTable[vga.herc.bgcolor];
		const uint32_t fgplane = FillTable[vga.herc.fgcolor];
		VGA_Latch pl;

		switch (vga.herc.write_mode) {
			case 0: // 0=bgcolor 1=fgcolor
				pl.d = (fgplane & valmask) + (bgplane & (~valmask));
				break;
			case 1: // 0=latch 1=fgcolor
				pl.d = (fgplane & valmask) + (vga.herc.latch & (~valmask));
				break;
			case 2: // 0=bgcolor 1=latch
				pl.d = (vga.herc.latch & valmask) + (bgplane & (~valmask));
				break;
			case 3: // 0=~latch 1=latch
			default:
				pl.d = vga.herc.latch ^ (~valmask);
				break;
		}

		((uint32_t*)vga.mem.linear)[start] = (((uint32_t*)vga.mem.linear)[start] & nochangemask) + (pl.d & (~nochangemask));
	}

	/* Your C++ compiler can probably optimize this down to just the optimal calls to based on type T... at least GCC does! */
	template <typename T=uint8_t> static INLINE T do_read(const PhysPt a) {
		if (sizeof(T) == 4) /* 32-bit to two 16-bit */
			return (uint32_t)do_read<uint16_t>(a) + ((uint32_t)do_read<uint16_t>(a+2) << (uint32_t)16u);
		else if (sizeof(T) == 2) /* 16-bit to two 8-bit */
			return (uint16_t)do_read<uint8_t>(a) + ((uint16_t)do_read<uint8_t>(a+1) << (uint32_t)8u);
		else
			return (T)readHandler(map(a));
	}
	template <typename T=uint8_t> static INLINE void do_write(const PhysPt a,const T v) {
		if (sizeof(T) == 4) /* split 32-bit to two 16-bit */
			{ do_write<uint16_t>(a,uint16_t(v)); do_write<uint16_t>(a+2,uint16_t(v >> (T)16u)); }
		else if (sizeof(T) == 2) /* split 16-bit to two 8-bit */
			{ do_write<uint8_t>(a,uint8_t(v)); do_write<uint8_t>(a+1,uint8_t(v >> (T)8u)); }
		else
			writeHandler(map(a),v);
	}

	uint8_t readb(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint8_t>(addr);
	}
	uint16_t readw(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint16_t>(addr);
	}
	uint32_t readd(PhysPt addr) override {
		VGAMEM_USEC_read_delay(); return do_read<uint32_t>(addr);
	}
	void writeb(PhysPt addr, uint8_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint8_t>(addr,val);
	}
	void writew(PhysPt addr,uint16_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint16_t>(addr,val);
	}
	void writed(PhysPt addr,uint32_t val) override {
		VGAMEM_USEC_write_delay(); do_write<uint32_t>(addr,val);
	}
};

static struct vg {
	VGA_PC98_LFB_Handler				map_lfb_pc98;
	VGA_SlowLFBHandler<VGA_PC98_LFB_Handler>	map_lfb_pc98_slow;
	VGA_Map_Handler					map;
	VGA_SlowLFBHandler<VGA_Map_Handler>		map_slow;
	VGA_Slow_CGA_Handler				slow;
	VGA_CGATEXT_PageHandler				cgatext;
	VGA_MCGATEXT_PageHandler			mcgatext;
	VGA_TANDY_PageHandler				tandy;
	VGA_SlowLFBHandler<VGA_TANDY_PageHandler>	tandy_slow;
	VGA_ChainedVGA_Handler				cvga;
	VGA_ChainedVGA_Slow_Handler			cvga_slow;
	VGA_ET4000_ChainedVGA_Slow_Handler		cvga_et4000_slow;
	VGA_UnchainedVGA_Handler			uvga;
	VGA_UnchainedVGA_Fast_Handler			uvga_fast;
	VGA_PCJR_Handler				pcjr;
	VGA_SlowLFBHandler<VGA_PCJR_Handler>		pcjr_slow;
	VGA_HERC_Handler				herc;
	VGA_SlowLFBHandler<VGA_HERC_Handler>		herc_slow;
	HERC_InColor_Mono_Handler			herc_incolor_mono;
	HERC_InColor_Graphics_Handler			herc_incolor_graphics;
	VGA_LFB_Handler					lfb;
	VGA_SlowLFBHandler<VGA_LFB_Handler>		lfb_slow;
	VGA_MMIO_Handler				mmio;
	VGA_AMS_Handler					ams;
	VGA_PC98_PageHandler				pc98;
	VGA_PC98_TEXT_PageHandler			pc98_text;
	VGA_PC98_CG_PageHandler				pc98_cg;
	VGA_PC98_256MMIO_PageHandler			pc98_256mmio;
	VGA_PC98_256BANK_PageHandler<0>			pc98_256bank0;
	VGA_PC98_256BANK_PageHandler<1>			pc98_256bank1;
	VGA_PC98_256Planar_PageHandler			pc98_256planar;
	VGA_Empty_Handler				empty;
} vgaph;

/* backdoor PC-98 memory I/O interface for GDC drawing code in vga_pc98_gdc_draw.cpp.
 * The GDC drawing code must not access video memory through CPU memory I/O functions
 * to avoid needless address translation and possible emulation stability issues that
 * can arise if the CPU is running in 386 protected mode and paging is enabled.
 *
 * According to nanshiki, the GDC's read/modify/write I/O may also go through the EGC
 * circuitry because there are references that mention using EGC functions to enhance
 * GDC draw commands. Which means just directly reading/writing planar memory will not
 * emulate things correctly. [https://github.com/joncampbell123/dosbox-x/pull/3508#issuecomment-1135452117]
 *
 * The code submitted by nanshiki already adds the drawing base address to the address
 * when calling these functions (e.g. A8000, B0000, B8000, E0000) so there is no need
 * to adjust them.
 *
 * This code calls readc/writec directly because read/write must translate CPU virtual
 * addresses to physical due to a design inherited from DOSBox SVN in which the address
 * is NOT translated to physical by the underlying emulator code before calling this
 * interface. If I had designed this, read/write would concern themselves only with
 * physical addresses. --Jonathan C. */
uint8_t pc98_gdc_vread(const uint32_t addr) {
	return vgaph.pc98.readc<uint8_t>(addr);
}

void pc98_gdc_vwrite(const uint32_t addr,const uint8_t b) {
	return vgaph.pc98.writec<uint8_t>(addr,b);
}

uint16_t pc98_gdc_vreadw(const uint32_t addr) {
	return vgaph.pc98.readc<uint16_t>(addr);
}

void pc98_gdc_vwritew(const uint32_t addr,const uint16_t b) {
	return vgaph.pc98.writec<uint16_t>(addr,b);
}

void VGA_ChangedBank(void) {
	VGA_SetupHandlers();
}

void MEM_ResetPageHandler_Unmapped(Bitu phys_page, Bitu pages);
void MEM_ResetPageHandler_RAM(Bitu phys_page, Bitu pages);

extern void DISP2_SetPageHandler(void);
void VGA_SetupHandlers(void) {
	/* This code inherited from DOSBox SVN confuses bank size with bank granularity.
	 * This code is enforcing bank granularity. Bank size concerns how much memory is
	 * mapped to A0000-BFFFF. Most cards expose a window of 64KB. Most cards also have
	 * a bank granularity of 64KB, but some, like Paradise and Cirrus, have 64KB windows
	 * and 4KB granularity. */
	vga.svga.bank_read_full = vga.svga.bank_read*vga.svga.bank_size;
	vga.svga.bank_write_full = vga.svga.bank_write*vga.svga.bank_size;
	bool runeten = false;
	PageHandler *newHandler;
	switch (machine) {
	case MCH_CGA:
		MEM_ResetPageHandler_Unmapped( VGA_PAGE_B0, 8 );            // B0000-B7FFF is unmapped
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
		MEM_SetPageHandler( VGA_PAGE_B8, 8, vga_memio_lfb_delay ? &vgaph.pcjr_slow : &vgaph.pcjr );
		goto range_done;
	case MCH_MDA:
	case MCH_HERC:
//		MEM_SetPageHandler( VGA_PAGE_A0, 16, &vgaph.empty );        // already done by VGA setup memory function, also interferes with "allow more than 640KB" option
		vgapages.base=VGA_PAGE_B0;
		/* NTS: Implemented according to [http://www.seasip.info/VintagePC/hercplus.html#regs] */
		if (vga.herc.enable_bits & 0x2) { /* bit 1: page in upper 32KB */
			/* NTS: I don't know what Hercules graphics cards do if you set bit 1 but not bit 0.
			 *      For the time being, I'm assuming that they respond to 0xB8000+ because of bit 1
			 *      but only map to the first 4KB because of bit 0. Basically, a configuration no
			 *      software would actually use. */
			if (vga.herc.enable_bits & 0x1) { /* allow graphics and enable 0xB1000-0xB7FFF */
				vgapages.mask=0xffff;
				MEM_SetPageHandler(VGA_PAGE_B0,16,(machine == MCH_HERC && hercCard == HERC_InColor)?(PageHandler*)(&vgaph.herc_incolor_graphics):(PageHandler*)(vga_memio_lfb_delay ? &vgaph.map_slow : &vgaph.map));
			}
			else {
				vgapages.mask=0xfff;
				MEM_SetPageHandler(VGA_PAGE_B0,16,(machine == MCH_HERC && hercCard == HERC_InColor)?(PageHandler*)(&vgaph.herc_incolor_graphics):(PageHandler*)(vga_memio_lfb_delay ? &vgaph.herc_slow : &vgaph.herc));
			}
		} else {
			// With hercules in 32kB mode it leaves a memory hole on 0xb800
			// and has MDA-compatible address wrapping when graphics are disabled
			if (vga.herc.enable_bits & 0x1) {
				vgapages.mask=0x7fff;
				MEM_SetPageHandler(VGA_PAGE_B0,16,(machine == MCH_HERC && hercCard == HERC_InColor)?(PageHandler*)(&vgaph.herc_incolor_graphics):(PageHandler*)(vga_memio_lfb_delay ? &vgaph.map_slow : &vgaph.map));
			}
			else {
				vgapages.mask=0xfff;
				MEM_SetPageHandler(VGA_PAGE_B0,16,(machine == MCH_HERC && hercCard == HERC_InColor)?(PageHandler*)(&vgaph.herc_incolor_graphics):(PageHandler*)(vga_memio_lfb_delay ? &vgaph.herc_slow : &vgaph.herc));
			}
			MEM_SetPageHandler(VGA_PAGE_B8,8,&vgaph.empty);
		}
		if (machine == MCH_HERC && hercCard == HERC_InColor && !(vga.herc.mode_control&0x2)/*not in graphics mode (bit 1 of 3b8h)*/) {
			// Text area in the first 16KB is less subject to planar functions
			MEM_SetPageHandler(VGA_PAGE_B0,4,&vgaph.herc_incolor_mono);
		}
		goto range_done;
	case MCH_TANDY:
		/* Always map 0xa000 - 0xbfff, might overwrite 0xb800 */
		vgapages.base=VGA_PAGE_A0;
		vgapages.mask=0x1ffff;
		MEM_SetPageHandler(VGA_PAGE_A0, 32, vga_memio_lfb_delay ? &vgaph.map_slow : &vgaph.map );
		if ( vga.tandy.extended_ram & 1 ) {
			//You seem to be able to also map different 64kb banks, but have to figure that out
			//This seems to work so far though
			vga.tandy.draw_base = vga.mem.linear;
			vga.tandy.mem_base = vga.mem.linear;
		} else {
			vga.tandy.draw_base = TANDY_VIDBASE( vga.tandy.draw_bank * 16 * 1024);
			vga.tandy.mem_base = TANDY_VIDBASE( vga.tandy.mem_bank * 16 * 1024);
			MEM_SetPageHandler( VGA_PAGE_B8, 8, vga_memio_lfb_delay ? &vgaph.tandy_slow : &vgaph.tandy );
		}
		goto range_done;
//		MEM_SetPageHandler(vga.tandy.mem_bank<<2,vga.tandy.is_32k_mode ? 0x08 : 0x04,range_handler);
	case MCH_AMSTRAD: // Memory handler.
		MEM_SetPageHandler( VGA_PAGE_B8, 4, &vgaph.ams );   // 0xb8000 - 0xbbfff
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

		/* If the system has 15MB or less RAM, OR the ISA memory hole at 15MB is enabled */
		if (MEM_TotalPages() <= 0xF00 || isa_memory_hole_15mb) {
			/* F00000-FF7FFFh linear framebuffer (256-packed)
			 *  - Does not exist except in 256-color mode.
			 *  - Switching from 256-color mode immediately unmaps this linear framebuffer.
			 *  - Switching to 256-color mode will immediately map the linear framebuffer if the enable bit is set in the PEGC MMIO registers */
			if ((pc98_gdc_vramop & (1 << VOPBIT_VGA)) && pc98_pegc_linear_framebuffer_enabled())
				MEM_SetPageHandler(0xF00, 512/*kb*/ / 4/*kb*/, vga_memio_lfb_delay ? &vgaph.map_lfb_pc98_slow : &vgaph.map_lfb_pc98 );
			else
				MEM_ResetPageHandler_Unmapped(0xF00, 512/*kb*/ / 4/*kb*/);
		}

		/* There is a confirmed alias of the PEGC framebuffer at 0xFFF00000-0xFFF7FFFF.
		 * Some PC-9821 CD-ROM games require this alias or else they display nothing on the screen.
		 * These games run in 32-bit protected mode and for whatever reason direct the DPMI server
		 * to map linear address 0xF00000-0xF7FFFF to physical memory address 0xFFF00000-0xFFF7FFFF,
		 * so that they can write to the linear address just as if a 386SX where that mapping existed.
		 * This is basically the same idea as "just under the BIOS at top of CPU addressable RAM"
		 * so if memalias is more than 24 bits map it to (1 << memalias) - 1MB to be consistent
		 * because that address will map to wherever PEGC's LFB is even if, say, running on a 486SX
		 * with 26 address pins. */
		if (MEM_get_address_bits() > 24) {
			Bitu page = ((Bitu)1 << (Bitu)(MEM_get_address_bits4GB() - 12/*pages not bytes*/)) - (Bitu)0x100/*1MB in pages*/;
			if ((pc98_gdc_vramop & (1 << VOPBIT_VGA)) && pc98_pegc_linear_framebuffer_enabled())
				MEM_SetPageHandler(page, 512/*kb*/ / 4/*kb*/, vga_memio_lfb_delay ? &vgaph.map_lfb_pc98_slow : &vgaph.map_lfb_pc98 );
			else
				MEM_ResetPageHandler_Unmapped(page, 512/*kb*/ / 4/*kb*/);
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
			newHandler = vga_memio_lfb_delay ? &vgaph.map_slow : &vgaph.map;
			break;
		case M_TEXT:
		case M_CGA2:
		case M_CGA4:
		case M_DCGA:
			/* EGA/VGA emulate CGA modes as chained */
			/* fall through */
		case M_LIN8:
		case M_LIN4:
		case M_VGA:
		case M_EGA:
			if (vga.config.chained) {
				if (vga.config.compatible_chain4) {
					if (vga.complexity.flags == 0 && memio_complexity_optimization) {
						/* The DOS program is not using anything beyond basic memory I/O and does not need the
						 * raster op, data rotate, and bit planar features at all. Therefore VGA memory I/O
						 * performance can be improved by assigning a simplified handler that omits that logic */
						if (svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K)
							newHandler = vga_memio_lfb_delay ? &vgaph.map_slow : &vgaph.map;
						else
							newHandler = &vgaph.cvga;
					}
					else {
						/* NTS: ET4000AX cards appear to have a different chain4 implementation from everyone else:
						 *      the planar memory byte address is address >> 2 and bits A0-A1 select the plane,
						 *      where all other clones I've tested seem to write planar memory byte (address & ~3)
						 *      (one byte per 4 bytes) and bits A0-A1 select the plane. Note that the et4000 emulation
						 *      implemented so far will not trigger this if() condition for 256-color mode. */
						/* FIXME: Different chain4 implementation on ET4000 noted---is it true also for ET3000? */
						if (svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K)
							newHandler = &vgaph.cvga_et4000_slow;
						else
							newHandler = &vgaph.cvga_slow;
					}
				}
				else if ((vga.complexity.flags != 0 || !memio_complexity_optimization) && (svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K) && vga.mode == M_VGA) {
					/* NTS: Tseng ET4000AX emulation CLEARS the "compatible chain4" flag for 256-color mode which is why this extra check is needed */
					newHandler = &vgaph.cvga_et4000_slow;
				}
				else {
					/* this is needed for SVGA modes (Paradise, Tseng, S3) because SVGA
					 * modes do NOT use the chain4 configuration. For Tseng ET4000AX
					 * emulation this map handler also handles chained 256-color mode
					 * because of the different way that the memory address is mapped
					 * to bitplane. */
					newHandler = vga_memio_lfb_delay ? &vgaph.map_slow : &vgaph.map;
				}
			} else {
				if (vga.complexity.flags == 0 && memio_complexity_optimization && (vga.mode == M_EGA || vga.mode == M_VGA))
					newHandler = &vgaph.uvga_fast;
				else
					newHandler = &vgaph.uvga;
			}
			break;
		case M_AMSTRAD:
			newHandler = vga_memio_lfb_delay ? &vgaph.map_slow : &vgaph.map;
			break;
	}
	// Workaround for ETen Chinese DOS system (e.g. ET24VA)
	if ((dos.loaded_codepage == 936 || dos.loaded_codepage == 950 || dos.loaded_codepage == 951) && strlen(RunningProgram) > 3 && !strncmp(RunningProgram, "ET", 2)) enveten = true;
	runeten = !vga_fill_inactive_ram && enveten && (dos.loaded_codepage == 936 || dos.loaded_codepage == 950 || dos.loaded_codepage == 951) && ((strlen(RunningProgram) > 3 && !strncmp(RunningProgram, "ET", 2)) || !TTF_using());
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
			if (CurMode && CurMode->mode >= 0x14/*VESA BIOS or extended mode*/ && vbe_window_size > 0/*user override of window size*/) {
				unsigned int pages = (vbe_window_size + 0xFFFu) >> 12u; /* bytes to pages, round up */
				if (pages > 32) pages = 32;
				assert(pages != 0u);

				/* map only what the window size determines, make the rest empty */
				MEM_SetPageHandler(VGA_PAGE_A0, pages, newHandler );
				MEM_SetPageHandler(VGA_PAGE_A0 + pages, 32 - pages, &vgaph.empty );
			}
			else {
				/*full 128KB */
				MEM_SetPageHandler(VGA_PAGE_A0, 32, newHandler );
			}
			break;
		case 1:
			vgapages.base = VGA_PAGE_A0;
			vgapages.mask = 0xffff & vga.mem.memmask;
			MEM_SetPageHandler( VGA_PAGE_A0, 16, newHandler );
			if (vga_fill_inactive_ram || runeten)
				MEM_ResetPageHandler_RAM( VGA_PAGE_B0, 16);
			else
				MEM_SetPageHandler( VGA_PAGE_B0, 16, &vgaph.empty );
			break;
		case 2:
			vgapages.base = VGA_PAGE_B0;
			vgapages.mask = 0x7fff & vga.mem.memmask;
			MEM_SetPageHandler( VGA_PAGE_B0, 8, newHandler );
			if (vga_fill_inactive_ram || runeten) {
				MEM_ResetPageHandler_RAM( VGA_PAGE_A0, 16 );
				MEM_ResetPageHandler_RAM( VGA_PAGE_B8, 8 );
			} else {
				MEM_SetPageHandler( VGA_PAGE_A0, 16, &vgaph.empty );
				MEM_SetPageHandler( VGA_PAGE_B8, 8, &vgaph.empty );
			}
			break;
		case 3:
			vgapages.base = VGA_PAGE_B8;
			vgapages.mask = 0x7fff & vga.mem.memmask;
			MEM_SetPageHandler( VGA_PAGE_B8, 8, newHandler );
			if (vga_fill_inactive_ram || runeten) {
				MEM_ResetPageHandler_RAM( VGA_PAGE_A0, 16 );
				MEM_ResetPageHandler_RAM( VGA_PAGE_B0, 8 );
			} else {
				MEM_SetPageHandler( VGA_PAGE_A0, 16, &vgaph.empty );
				MEM_SetPageHandler( VGA_PAGE_B0, 8, &vgaph.empty );
			}
			break;
	}
	if(svgaCard == SVGA_S3Trio && (vga.s3.ext_mem_ctrl & 0x10))
		MEM_SetPageHandler(VGA_PAGE_A0, 16, &vgaph.mmio);

	/* NTS: Based on a discussion on GitHub [https://github.com/joncampbell123/dosbox-x/issues/4042] and
	 *      on Vogons [https://www.vogons.org/viewtopic.php?t=45808] it makes logical sense that Chain 4
	 *      would take precedence over odd/even mode. There are some DOS games that accidentally clear
	 *      bit 2 and would normally enable odd/even mode in 256-color mode, but they displayed correctly
	 *      anyway. This fixes "Seal: Magic Eye" and a 1994 256-byte Demoscene entry.
	 *
	 *      If chain 4 (or "compatible chain 4") is enabled, then ignore odd/even mode.
	 *      If neither text nor CGA display mode and requested by the user through dosbox.conf, ignore odd/even mode. */
	non_cga_ignore_oddeven_engage =
		(vga.seq.memory_mode & 8/*Chain 4 enable*/) ||
		(non_cga_ignore_oddeven && !(vga.mode == M_TEXT || vga.mode == M_CGA2 || vga.mode == M_CGA4));

range_done:
#if C_DEBUG
	if (control->opt_display2) DISP2_SetPageHandler();
#endif
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

	/* NTS: 64KB winsz = 0x10000 => winmsk = 0xFFFF0000 => 0xFFFF
	 *      1MB winsz = 0x100000 => winmsk = 0xFFF00000 => 0xFFF0
	 *      2MB winsz = 0x200000 => winmsk = 0xFFE00000 => 0xFFE0
	 *      and so on.
	 *
	 * From the S3 Trio32/Trio64 documentation regarding the Linear Address Window Position Registers:
	 * "The Linear Address Window resides on a 64KB, 1MB, 2MB, or 4MB (Trio64 only) memory boundary (size-aligned) ...
	 *  Some LSBs of this register are ignored because of the size-aligned boundary scheme" */
	const unsigned int la_winmsk = ~((winsz - 1u) >> 16u); /* Register holds the upper 16 bits of the linear address */

	/* The LFB register has an enable bit */
	if (!(vga.s3.reg_58 & 0x10)) {
		vga.lfb.page = (unsigned int)(vga.s3.la_window & la_winmsk) << 4u;
		vga.lfb.addr = (unsigned int)(vga.s3.la_window & la_winmsk) << 16u;
		vga.lfb.handler = NULL;
		MEM_SetLFB(0,0,NULL,NULL);
	}
	/* if the DOS application or Windows 3.1 driver attempts to put the linear framebuffer
	 * below the top of memory, then we're probably entering a DOS VM and it's probably
	 * a 64KB window. If it's not a 64KB window then print a warning. */
	else if ((unsigned long)(vga.s3.la_window << 4UL) < (unsigned long)MEM_TotalPages()) {
		if (winsz != 0x10000) // 64KB window normal for entering a DOS VM in Windows 3.1 or legacy bank switching in DOS
			LOG(LOG_MISC,LOG_WARN)("S3 warning: Window size != 64KB and address conflict with system RAM!");

		vga.lfb.page = (unsigned int)(vga.s3.la_window & la_winmsk) << 4u;
		vga.lfb.addr = (unsigned int)(vga.s3.la_window & la_winmsk) << 16u;
		vga.lfb.handler = NULL;
		MEM_SetLFB(0,0,NULL,NULL);
	}
	else {
		vga.lfb.page = (unsigned int)(vga.s3.la_window & la_winmsk) << 4u;
		vga.lfb.addr = (unsigned int)(vga.s3.la_window & la_winmsk) << 16u;
		vga.lfb.handler = vga_memio_lfb_delay ? &vgaph.lfb_slow : &vgaph.lfb;
		MEM_SetLFB((unsigned int)(vga.s3.la_window & la_winmsk) << 4u,(unsigned int)vga.mem.memsize/4096u, vga.lfb.handler, &vgaph.mmio);
	}
}

static bool VGA_Memory_ShutDown_init = false;

static void VGA_Memory_ShutDown(Section * /*sec*/) {
	if (machine == MCH_CGA)
		MEM_SetPageHandler(VGA_PAGE_B8,8,&vgaph.empty);
	else if (machine == MCH_HERC || machine == MCH_MDA)
		MEM_SetPageHandler(VGA_PAGE_B0,8,&vgaph.empty);
	else
		MEM_SetPageHandler(VGA_PAGE_A0,32,&vgaph.empty);

#if C_DEBUG
	if (control->opt_display2)
		MEM_SetPageHandler(VGA_PAGE_B0,8,&vgaph.empty);
#endif

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

        /* may be related */
        VGA_SetupHandlers();
    }

	vga.svga.bank_read = vga.svga.bank_write = 0;
	vga.svga.bank_read_full = vga.svga.bank_write_full = 0;

	/* obey user override for "bank size", which this code inherited from DOSBox SVN
	 * confuses with "bank granularity". If "bank size" were truly a concern it would
	 * affect how much of the A0000-BFFFF region VGA mapping would expose. */
	if (vbe_window_granularity > 0)
		vga.svga.bank_size = vbe_window_granularity; /* allow different sizes for dev testing */
	else
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
