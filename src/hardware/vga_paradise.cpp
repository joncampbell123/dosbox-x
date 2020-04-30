/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */


#include "dosbox.h"
#include "setup.h"
#include "vga.h"
#include "inout.h"
#include "mem.h"

typedef struct {
	Bitu PR0A;
	Bitu PR0B;
	Bitu PR1;
	Bitu PR2;
	Bitu PR3;
	Bitu PR4;
	Bitu PR5;

	inline bool locked() { return (PR5&7)!=5; }

	Bitu clockFreq[4];
	Bitu biosMode;
} SVGA_PVGA1A_DATA;

static SVGA_PVGA1A_DATA pvga1a = { 0,0, 0,0,0,0,0, {0,0,0,0}, 0 };


static void bank_setup_pvga1a() {
// Note: There is some inconsistency in available documentation. Most sources tell that PVGA1A used
//       only 7 bits of bank index (VGADOC and Ferraro agree on that) but also point that there are
//       implementations with 1M of RAM which is simply not possible with 7-bit banks. This implementation
//       assumes that the eighth bit was actually wired and could be used. This does not conflict with
//       anything and actually works in WHATVGA just fine.
	if (pvga1a.PR1 & 0x08) {
		// TODO: Dual bank function is not supported yet
		// TODO: Requirements are not compatible with vga_memory implementation.
	} else {
		// Single bank config is straightforward
		vga.svga.bank_read = vga.svga.bank_write = (Bit8u)pvga1a.PR0A;
		vga.svga.bank_size = 4*1024;
		VGA_SetupHandlers();
	}
}

void write_p3cf_pvga1a(Bitu reg,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
	if (pvga1a.locked() && reg >= 0x09 && reg <= 0x0e)
		return;

	switch (reg) {
	case 0x09:
		// Bank A, 4K granularity, not using bit 7
		// Maps to A800h-AFFFh if PR1 bit 3 set and 64k config B000h-BFFFh if 128k config. A000h-AFFFh otherwise.
		pvga1a.PR0A = val;
		bank_setup_pvga1a();
		break;
	case 0x0a:
		// Bank B, 4K granularity, not using bit 7
		// Maps to A000h-A7FFh if PR1 bit 3 set and 64k config, A000h-AFFFh if 128k
		pvga1a.PR0B = val;
		bank_setup_pvga1a();
		break;
	case 0x0b:
		// Memory size. We only allow to mess with bit 3 here (enable bank B) - this may break some detection schemes
		pvga1a.PR1 = (pvga1a.PR1 & ~0x08u) | (val & 0x08u);
		bank_setup_pvga1a();
		break;
	case 0x0c:
		// Video configuration
		// TODO: Figure out if there is anything worth implementing here.
		pvga1a.PR2 = val;
		break;
	case 0x0d:
		// CRT control. Bits 3-4 contain bits 16-17 of CRT start.
		// TODO: Implement bit 2 (CRT address doubling - this mechanism is present in other chipsets as well,
		// but not implemented in DosBox core)
		pvga1a.PR3 = val;
		vga.config.display_start = (vga.config.display_start & 0xffffu) | ((val & 0x18u)<<13u);
		vga.config.cursor_start = (vga.config.cursor_start & 0xffffu) | ((val & 0x18u)<<13u);
		break;
	case 0x0e:
		// Video control
		// TODO: Figure out if there is anything worth implementing here.
		pvga1a.PR4 = val;
		break;
	case 0x0f:
		// Enable extended registers
		pvga1a.PR5 = val;
		break;
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:GFX:PVGA1A:Write to illegal index %2X", (int)reg);
		break;
	}
}

Bitu read_p3cf_pvga1a(Bitu reg,Bitu iolen) {
    (void)iolen;//UNUSED
	if (pvga1a.locked() && reg >= 0x09 && reg <= 0x0e)
		return 0x0;

	switch (reg) {
	case 0x09:
		return pvga1a.PR0A;
	case 0x0a:
		return pvga1a.PR0B;
	case 0x0b:
		return pvga1a.PR1;
	case 0x0c:
		return pvga1a.PR2;
	case 0x0d:
		return pvga1a.PR3;
	case 0x0e:
		return pvga1a.PR4;
	case 0x0f:
		return pvga1a.PR5;
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:GFX:PVGA1A:Read from illegal index %2X", (int)reg);
		break;
	}

	return 0x0;
}

void FinishSetMode_PVGA1A(Bitu /*crtc_base*/, VGA_ModeExtraData* modeData) {
	pvga1a.biosMode = modeData->modeNo;

// Reset to single bank and set it to 0. May need to unlock first (DPaint locks on exit)
	IO_Write(0x3ce, 0x0f);
	Bitu oldlock = IO_Read(0x3cf);
	IO_Write(0x3cf, 0x05);
	IO_Write(0x3ce, 0x09);
	IO_Write(0x3cf, 0x00);
	IO_Write(0x3ce, 0x0a);
	IO_Write(0x3cf, 0x00);
	IO_Write(0x3ce, 0x0b);
	Bit8u val = IO_Read(0x3cf);
	IO_Write(0x3cf, val & ~0x08);
	IO_Write(0x3ce, 0x0c);
	IO_Write(0x3cf, 0x00);
	IO_Write(0x3ce, 0x0d);
	IO_Write(0x3cf, 0x00);
	IO_Write(0x3ce, 0x0e);
	IO_Write(0x3cf, 0x00);
	IO_Write(0x3ce, 0x0f);
	IO_Write(0x3cf, (Bit8u)oldlock);

	if (svga.determine_mode)
		svga.determine_mode();

	if(vga.mode != M_VGA) {
		vga.config.compatible_chain4 = false;
//		vga.vmemwrap = vga.mem.memsize;
	} else {
		vga.config.compatible_chain4 = true;
//		vga.vmemwrap = 256*1024;
	}

	VGA_SetupHandlers();
}

void DetermineMode_PVGA1A() {
	// Close replica from the base implementation. It will stay here
	// until I figure a way to either distinguish M_VGA and M_LIN8 or
	// merge them.
	if (vga.attr.mode_control & 1) {
		if (vga.gfx.mode & 0x40) VGA_SetMode((pvga1a.biosMode<=0x13)?M_VGA:M_LIN8);
		else if (vga.gfx.mode & 0x20) VGA_SetMode(M_CGA4);
		else if ((vga.gfx.miscellaneous & 0x0c)==0x0c) VGA_SetMode(M_CGA2);
		else VGA_SetMode((pvga1a.biosMode<=0x13)?M_EGA:M_LIN4);
	} else {
		VGA_SetMode(M_TEXT);
	}
}

void SetClock_PVGA1A(Bitu which,Bitu target) {
	if (which < 4) {
		pvga1a.clockFreq[which]=1000*target;
		VGA_StartResize();
	}
}

Bitu GetClock_PVGA1A() {
	return pvga1a.clockFreq[(vga.misc_output >> 2) & 3];
}

bool AcceptsMode_PVGA1A(Bitu mode) {
	return VideoModeMemSize(mode) < vga.mem.memsize;
}

void SVGA_Setup_ParadisePVGA1A(void) {
	svga.write_p3cf = &write_p3cf_pvga1a;
	svga.read_p3cf = &read_p3cf_pvga1a;

	svga.set_video_mode = &FinishSetMode_PVGA1A;
	svga.determine_mode = &DetermineMode_PVGA1A;
	svga.set_clock = &SetClock_PVGA1A;
	svga.get_clock = &GetClock_PVGA1A;
	svga.accepts_mode = &AcceptsMode_PVGA1A;

	VGA_SetClock(0,CLK_25);
	VGA_SetClock(1,CLK_28);
	VGA_SetClock(2,32400); // could not find documentation
	VGA_SetClock(3,35900);

	// Adjust memory, default to 512K
	if (vga.mem.memsize == 0)
		vga.mem.memsize = 512*1024;

	if (vga.mem.memsize < 512*1024)	{
		vga.mem.memsize = 256*1024;
		pvga1a.PR1 = 1<<6;
	} else if (vga.mem.memsize > 512*1024) {
		vga.mem.memsize = 1024*1024;
		pvga1a.PR1 = 3<<6;
	} else {
		pvga1a.PR1 = 2<<6;
	}

	// Paradise ROM signature
	PhysPt rom_base=PhysMake(0xc000,0);
	phys_writeb(rom_base+0x007d,'V');
	phys_writeb(rom_base+0x007e,'G');
	phys_writeb(rom_base+0x007f,'A');
	phys_writeb(rom_base+0x0080,'=');

	IO_Write(0x3cf, 0x05); // Enable!
}

