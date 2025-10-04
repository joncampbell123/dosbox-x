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

#define VGA_INTERNAL

#include "dosbox.h"
#include "inout.h"
#include "logging.h"
#include "vga.h"

extern bool ignore_sequencer_blanking;
extern bool non_cga_ignore_oddeven_engage;
extern bool vga_ignore_extended_memory_bit;

extern bool vga_render_on_demand;
void VGA_RenderOnDemandUpTo(void);

Bitu read_p3c4(Bitu /*port*/,Bitu /*iolen*/) {
	return seq(index);
}

void write_p3c4(Bitu /*port*/,Bitu val,Bitu /*iolen*/) {
	// FIXME: Standard VGA behavior (based on observation) is to latch
	//        the sequencer register based only on the lowest 4 bits.
	//        Thus, the map mask register is accessible not only by index 0x02,
	//        but also from 0x12, 0x22, etc..
	//
	//        This isn't quite universal, as WHATVGA documentation suggests
	//        there are SVGA chipsets with extended registers in the 0x07-0x17
	//        and 0x80-0x9F ranges. But register snapshots so far seem to suggest
	//        most cards just mask off the index to 4 bits and put the extended
	//        regs elsewhere.
	//
	//        The yet to be answered question is: if the card only latches the
	//        low 4 bits, then what will you see when you read back the sequencer
	//        index register? Will you see the full 8-bit value, or the low 4 bits
	//        it actually decoded?
	//
	// FIXME: I don't have an EGA to test with, but, what masking behavior here
	//        do EGA and compatibles do with the index?

	/* Emulating the mask behavior fixes the following problems with games & demoscene entries:
	 *
	 * - "Spiral" by Dataction: SPIRAL.EXE appears to use VGA Mode-X 256-color mode, but it relies
	 *   on sequencer alias register 0x12 for masking bitplanes instead of properly writing
	 *   Map Mask Register (register 0x02). Older VGA chipsets only decoded 3 or 4 bits of the sequencer
	 *   index register, so this happened to work anyway since (0x12 & 0x0F) == 0x02, but it also means
	 *   the demo will not render Mode X properly on newer SVGA chipsets that decode more than 4 bits of
	 *   the sequencer index register. Adding this masking behavior, and running the demo
	 *   with machine=svga_et4000 allows the demo to display properly instead of rendering as
	 *   a blocky low-res mess.
	 *   [http://www.pouet.net/prod.php?which=12630]
	 */

	if (machine == MCH_VGA) {
		if (svgaCard == SVGA_S3Trio)
			val &= 0x3F;	// observed behavior: only the low 6 bits
		else if (svgaCard == SVGA_TsengET4K)
			val &= 0x07;	// observed behavior: only the low 3 bits
		else if (svgaCard == SVGA_TsengET3K)
			val &= 0x07;	// FIXME: reasonable guess, since the ET4000 does it too
		else
			val &= 0x0F;	// FIXME: reasonable guess

        /* Paradise/Western Digital sequencer registers appear to repeat every 0x40 aka decoding bits [5:0] */
	}
	else if (machine == MCH_EGA) {
		val &= 0x0F; // FIXME: reasonable guess
	}

	seq(index)=(uint8_t)val;
}

unsigned int VGA_ComplexityCheck_MAP_MASK(void) {
	return vga.complexity.setf(VGACMPLX_MAP_MASK,(vga.seq.map_mask & 0xF) != 0xF && vga.config.chained); // if any bitplane is masked off and chained mode
}

unsigned int VGA_ComplexityCheck_ODDEVEN(void) {
	bool unusual = false;

	/* Ignore odd/even mode for 256-color mode if enabled (by default).
	 * It is a bug to enable odd/even in 256-color mode and most SVGA chipsets appear to
	 * completely ignore the bit in that situation (which is probably how the programmer
	 * of a specific demo did not catch the bug in their tweakmode). */
	if (non_cga_ignore_oddeven_engage && (vga.mode == M_VGA || vga.mode == M_LIN8)) {
		/* ignore */
	}
	else {
		/* NTS: Sequencer memory mode bit 2: Odd/even mode is SET when the bit is CLEARED, meaning that bit 2 is an
		 *      Odd/Even DISABLE bit, not an ENABLE bit. Some documentation including the WHATVGA documentation got it backwards.
		 *      When set, even addresses go to bitplane 0+2, odd addresses go to bitplane 1+3. Note this bit doesn't affect
		 *      the byte offset in planar video memory that is accessed when the CPU reads/writes video RAM.
		 *
		 *      Graphics controller misc graphics register bit 2: Odd/even mode is SET when the bit is SET. This bit
		 *      says to REPLACE bit 0 with a "higher order bit" and the odd map is selected.
		 *
		 *      (wait, is that how EGA cards do the 640x350 4-color mode when you only have 64KB of video RAM?) */
		unusual = ((vga.seq.memory_mode & 4) == 0) || ((vga.gfx.miscellaneous & 2) != 0);

		/* NOTE: DOSBox-X treats the CGA modes as just another variant of EGA 16-color mode (M_EGA) because that's how
		 *       real EGA/VGA hardware handles them too. The only difference from the standard EGA mode is that in the
		 *       CGA modes the additional bitplanes are disabled and not rendered, and the 320x200 4-color mode sets
		 *       an additional bit that instructs the planar mode to read 2-bit pixel values from bitplane 0 instead of
		 *       1-bit pixel values as normal. Since odd/even mode is enabled, alternate bytes of bitplane 0 and 1 are read.
		 *       What is normally hidden by the disabled bitplanes is that the 2-bit pixel mode also reads an additional
		 *       2 bits from bitplane 2 or bitplane 2 and 3 to produce a 4-bit (16-color value) which is hardly used
		 *       except for one known test case that sets up a 16-color 640x200 tweakmode for machine=ega in which the
		 *       display is two 2-bit/pixel bitplanes instead of four 1-bit/pixel bitplanes.
		 *
		 *       CGA modes 4/5 (320x200 4-color) are run with odd/even mode enabled and therefore need the slower I/O path to
		 *       function correctly.
		 *
		 *       EGA/VGA also use the Odd/Even mode for alphanumeric text mode for CGA compatibility reasons and because
		 *       it allows the hardware to then map the character data to bitplane 0 and attribute data to bitplane 1
		 *       while providing the illusion of those even/odd bytes for DOS programs written against CGA/MDA hardware.
		 *
		 *       Only the slow memory I/O handler can correctly map odd/even mode. */
	}

	return vga.complexity.setf(VGACMPLX_ODDEVEN,unusual);
}

void write_p3c5(Bitu /*port*/,Bitu val,Bitu iolen) {
	unsigned int cmplx = 0;

//	LOG_MSG("SEQ WRITE reg %X val %X",seq(index),val);
	switch(seq(index)) {
	case 0:		/* Reset */
		if (vga_render_on_demand) VGA_RenderOnDemandUpTo();
		seq(reset)=(uint8_t)val;
		break;
	case 1:		/* Clocking Mode */
		if (val!=seq(clocking_mode)) {
			if (vga_render_on_demand) VGA_RenderOnDemandUpTo();
			// don't resize if only the screen off bit was changed
			if ((val&(~0x20u))!=(seq(clocking_mode)&(~0x20u))) {
				seq(clocking_mode)=(uint8_t)val;
				VGA_StartResize();
			} else {
				seq(clocking_mode)=(uint8_t)val;
			}
			if ((val & 0x20) && !ignore_sequencer_blanking) vga.attr.disabled |= 0x2u;
			else vga.attr.disabled &= ~0x2u;
		}
		/* TODO Figure this out :)
			0	If set character clocks are 8 dots wide, else 9.
			2	If set loads video serializers every other character
				clock cycle, else every one.
			3	If set the Dot Clock is Master Clock/2, else same as Master Clock
				(See 3C2h bit 2-3). (Doubles pixels). Note: on some SVGA chipsets
				this bit also affects the Sequencer mode.
			4	If set loads video serializers every fourth character clock cycle,
				else every one.
			5	if set turns off screen and gives all memory cycles to the CPU
				interface.
		*/
		break;
	case 2:		/* Map Mask */
		seq(map_mask)=val & 15;
		vga.config.full_map_mask=FillTable[val & 15];
		vga.config.full_not_map_mask=~vga.config.full_map_mask;
		cmplx |= VGA_ComplexityCheck_MAP_MASK();
		if (cmplx != 0) VGA_SetupHandlers();
		/*
			0  Enable writes to plane 0 if set
			1  Enable writes to plane 1 if set
			2  Enable writes to plane 2 if set
			3  Enable writes to plane 3 if set
		*/
		break;
	case 3:		/* Character Map Select */
		{
			if (vga_render_on_demand) VGA_RenderOnDemandUpTo();
			seq(character_map_select)=(uint8_t)val;
			uint8_t font1=(val & 0x3) << 1;
			if (IS_VGA_ARCH) font1|=(val & 0x10) >> 4;
			vga.draw.font_tables[0]=vga.mem.linear + (((font1*8*1024) & vga.draw.planar_mask) * 4u/*planar byte to byte offset*/) + 2/*plane*/;
			uint8_t font2=((val & 0xc) >> 1);
			if (IS_VGA_ARCH) font2|=(val & 0x20) >> 5;
			vga.draw.font_tables[1]=vga.mem.linear + (((font2*8*1024) & vga.draw.planar_mask) * 4u/*planar byte to byte offset*/) + 2/*plane*/;
		}
		/*
			0,1,4  Selects VGA Character Map (0..7) if bit 3 of the character
					attribute is clear.
			2,3,5  Selects VGA Character Map (0..7) if bit 3 of the character
					attribute is set.
			Note: Character Maps are placed as follows:
			Map 0 at 0k, 1 at 16k, 2 at 32k, 3: 48k, 4: 8k, 5: 24k, 6: 40k, 7: 56k
		*/
		break;
	case 4:	/* Memory Mode */
		/* 
			0  Set if in an alphanumeric mode, clear in graphics modes.
			1  Set if more than 64kbytes on the adapter.
			2  Disables Odd/Even addressing mode if set. Odd/Even mode places all odd
				bytes in plane 1&3, and all even bytes in plane 0&2.
			3  If set address bit 0-1 selects video memory planes (256 color mode),
				rather than the Map Mask and Read Map Select Registers.
		*/
		seq(memory_mode)=(uint8_t)val;
		cmplx |= vga.complexity.setf(VGACMPLX_NON_EXTENDED,(val & 2) == 0 && !vga_ignore_extended_memory_bit); // only 64kb on the adapter?
		cmplx |= VGA_ComplexityCheck_ODDEVEN();
		if (IS_VGA_ARCH) {
			/* Changing this means changing the VGA Memory Read/Write Handler */
			if (val&0x08) vga.config.chained=true;
			else vga.config.chained=false;
			cmplx |= VGA_ComplexityCheck_MAP_MASK();
			VGA_SetupHandlers();
		}
		else if (cmplx != 0) {
			VGA_SetupHandlers();
		}
		break;
	default:
		if (svga.write_p3c5) {
			svga.write_p3c5(seq(index), val, iolen);
		} else {
			LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:SEQ:Write to illegal index %2X",seq(index));
		}
		break;
	}
}


Bitu read_p3c5(Bitu /*port*/,Bitu iolen) {
//	LOG_MSG("VGA:SEQ:Read from index %2X",seq(index));
	switch(seq(index)) {
	case 0:			/* Reset */
		return seq(reset);
		break;
	case 1:			/* Clocking Mode */
		return seq(clocking_mode);
		break;
	case 2:			/* Map Mask */
		return seq(map_mask);
		break;
	case 3:			/* Character Map Select */
		return seq(character_map_select);
		break;
	case 4:			/* Memory Mode */
		return seq(memory_mode);
		break;
	default:
		if (svga.read_p3c5)
			return svga.read_p3c5(seq(index), iolen);
		break;
	}
	return 0;
}


void VGA_SetupSEQ(void) {
	if (IS_EGAVGA_ARCH) {
		IO_RegisterWriteHandler(0x3c4,write_p3c4,IO_MB);
		IO_RegisterWriteHandler(0x3c5,write_p3c5,IO_MB);
		if (IS_VGA_ARCH) {
			IO_RegisterReadHandler(0x3c4,read_p3c4,IO_MB);
			IO_RegisterReadHandler(0x3c5,read_p3c5,IO_MB);
		}
	}
}

void VGA_UnsetupSEQ(void) {
    IO_FreeWriteHandler(0x3c4,IO_MB);
    IO_FreeReadHandler(0x3c4,IO_MB);
    IO_FreeWriteHandler(0x3c5,IO_MB);
    IO_FreeReadHandler(0x3c5,IO_MB);
}

// save state support
void POD_Save_VGA_Seq( std::ostream& stream )
{
	// - pure struct data
	WRITE_POD( &vga.seq, vga.seq );


	// no static globals found
}


void POD_Load_VGA_Seq( std::istream& stream )
{
	// - pure struct data
	READ_POD( &vga.seq, vga.seq );


	// no static globals found
}
