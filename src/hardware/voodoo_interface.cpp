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


#include <stdlib.h>
#include <math.h>

#include "dosbox.h"
#include "cross.h"

#include "vga.h"
#include "pic.h"
#include "paging.h"
#include "render.h"

#include "voodoo_interface.h"
#include "voodoo_emu.h"


static voodoo_draw vdraw;


Voodoo_PageHandler * voodoo_pagehandler;


uint8_t Voodoo_PageHandler::readb(PhysPt addr) {
    (void)addr;//UNUSED
//	LOG_MSG("voodoo readb at %x",addr);
	return (uint8_t)-1;
}
void Voodoo_PageHandler::writeb(PhysPt addr,uint8_t val) {
    (void)addr;//UNUSED
    (void)val;//UNUSED
//	LOG_MSG("voodoo writeb at %x",addr);
}

uint16_t Voodoo_PageHandler::readw(PhysPt addr) {
	addr = PAGING_GetPhysicalAddress(addr);
    if (addr&1) {
        LOG_MSG("voodoo readw unaligned");
        return (uint16_t)-1;
    }

	Bit32u retval = voodoo_r((addr>>2)&0x3FFFFF);
	if (addr&3)
		retval >>= 16;
	else
		retval &= 0xffff;

	return (uint16_t)retval;
}

void Voodoo_PageHandler::writew(PhysPt addr,uint16_t val) {
	addr = PAGING_GetPhysicalAddress(addr);
	if (addr&1u) {
        LOG_MSG("voodoo writew unaligned");
        return;
    }

	if (addr&3u)
		voodoo_w((addr>>2u)&0x3FFFFFu,(UINT32)(val<<16u),0xffff0000u);
	else
		voodoo_w((addr>>2u)&0x3FFFFFu,val,0x0000ffffu);
}

Bit32u Voodoo_PageHandler::readd(PhysPt addr) {
	addr = PAGING_GetPhysicalAddress(addr);
	if (!(addr&3)) {
		return voodoo_r((addr>>2)&0x3FFFFF);
	} else {
		if (!(addr&1)) {
			Bit32u low = voodoo_r((addr>>2)&0x3FFFFF);
			Bit32u high = voodoo_r(((addr>>2)+1)&0x3FFFFF);
			return (low>>16) | (high<<16);
		} else {
			LOG_MSG("voodoo readd unaligned");
		}
	}
	return 0xffffffff;
}

void Voodoo_PageHandler::writed(PhysPt addr,Bit32u val) {
	addr = PAGING_GetPhysicalAddress(addr);
	if (!(addr&3)) {
		voodoo_w((addr>>2)&0x3FFFFF,val,0xffffffff);
	} else {
		if (!(addr&1)) {
			voodoo_w((addr>>2)&0x3FFFFF,val<<16,0xffff0000);
			voodoo_w(((addr>>2)+1)&0x3FFFFF,val>>16,0x0000ffff);
		} else {
			Bit32u val1 = voodoo_r((addr>>2)&0x3FFFFF);
			Bit32u val2 = voodoo_r(((addr>>2)+1)&0x3FFFFF);
			if ((addr&3)==1) {
				val1 = (val1&0xffffff) | ((val&0xff)<<24);
				val2 = (val2&0xff000000) | (val>>8);
			} else if ((addr&3)==3) {
				val1 = (val1&0xff) | ((val&0xffffff)<<8);
				val2 = (val2&0xffffff00) | (val>>24);
			} else E_Exit("???");
			voodoo_w((addr>>2)&0x3FFFFF,val1,0xffffffff);
			voodoo_w(((addr>>2)+1)&0x3FFFFF,val2,0xffffffff);
		}
	}
}


static void Voodoo_VerticalTimer(Bitu /*val*/) {
	vdraw.frame_start = PIC_FullIndex();
	PIC_AddEvent( Voodoo_VerticalTimer, vdraw.vfreq );

	if (v->fbi.vblank_flush_pending) {
		voodoo_vblank_flush();
		if (GFX_LazyFullscreenRequested()) {
			v->ogl_dimchange = true;
		}
	}

	if (!v->ogl) {
		if (!RENDER_StartUpdate()) return; // frameskip

		rectangle r;
		r.min_x = r.min_y = 0;
		r.max_x = (int)v->fbi.width;
		r.max_y = (int)v->fbi.height;

		// draw all lines at once
		uint16_t *viewbuf = (uint16_t *)(v->fbi.ram + v->fbi.rgboffs[v->fbi.frontbuf]);
		for(Bitu i = 0; i < v->fbi.height; i++) {
			RENDER_DrawLine((uint8_t*) viewbuf);
			viewbuf += v->fbi.rowpixels;
		}
		RENDER_EndUpdate(false);
	} else {
		// ???
		voodoo_set_window();
	}
}

bool Voodoo_GetRetrace() {
	// TODO proper implementation
	double time_in_frame = PIC_FullIndex() - vdraw.frame_start;
	double vfreq = vdraw.vfreq;
	if (vfreq <= 0.0) return false;
	if (v->clock_enabled && v->output_on) {
		if ((time_in_frame/vfreq) > 0.95) return true;
	} else if (v->output_on) {
		double rtime = time_in_frame/vfreq;
		rtime = fmod(rtime, 1.0);
		if (rtime > 0.95) return true;
	}
	return false;
}

double Voodoo_GetVRetracePosition() {
	// TODO proper implementation
	double time_in_frame = PIC_FullIndex() - vdraw.frame_start;
	double vfreq = vdraw.vfreq;
	if (vfreq <= 0.0) return 0.0;
	if (v->clock_enabled && v->output_on) {
		return time_in_frame/vfreq;
	} else if (v->output_on) {
		double rtime = time_in_frame/vfreq;
		rtime = fmod(rtime, 1.0);
		return rtime;
	}
	return 0.0;
}

double Voodoo_GetHRetracePosition() {
	// TODO proper implementation
	double time_in_frame = PIC_FullIndex() - vdraw.frame_start;
	double hfreq = vdraw.vfreq*100.0;
	if (hfreq <= 0.0) return 0.0;
	if (v->clock_enabled && v->output_on) {
		return time_in_frame/hfreq;
	} else if (v->output_on) {
		double rtime = time_in_frame/hfreq;
		rtime = fmod(rtime, 1.0);
		return rtime;
	}
	return 0.0;
}

static void Voodoo_UpdateScreen(void) {
	// abort drawing
	RENDER_EndUpdate(true);

	if ((!v->clock_enabled || !v->output_on) && vdraw.override_on) {
		// switching off
		PIC_RemoveEvents(Voodoo_VerticalTimer);
		voodoo_leave();

		VGA_SetOverride(false);
		vdraw.override_on=false;
	}

	if ((v->clock_enabled && v->output_on) && !vdraw.override_on) {
		// switching on
		PIC_RemoveEvents(Voodoo_VerticalTimer); // shouldn't be needed
		
		// TODO proper implementation of refresh rates and timings
		vdraw.vfreq = 1000.0f/60.0f;
		VGA_SetOverride(true);
		vdraw.override_on=true;

		vdraw.height=v->fbi.height;

		voodoo_activate();
		
		if (v->ogl) {
			v->ogl_dimchange = false;
		} else {
			RENDER_SetSize(v->fbi.width, v->fbi.height, 16, 1000.0f / vdraw.vfreq, 4.0/3.0);
		}

		Voodoo_VerticalTimer(0);
	}

	if ((v->clock_enabled && v->output_on) && v->ogl_dimchange) {
		voodoo_update_dimensions();
	}

	vdraw.screen_update_requested = false;
}

static void Voodoo_CheckScreenUpdate(Bitu /*val*/) {
	vdraw.screen_update_pending = false;
	if (vdraw.screen_update_requested) {
		vdraw.screen_update_pending = true;
		Voodoo_UpdateScreen();
		PIC_AddEvent(Voodoo_CheckScreenUpdate, 100.0f);
	}
}

void Voodoo_UpdateScreenStart() {
	vdraw.screen_update_requested = true;
	if (!vdraw.screen_update_pending) {
		vdraw.screen_update_pending = true;
		PIC_AddEvent(Voodoo_CheckScreenUpdate, 0.0f);
	}
}

void Voodoo_Output_Enable(bool enabled) {
	if (v->output_on != enabled) {
		v->output_on = enabled;
		Voodoo_UpdateScreenStart();
	}
}

void Voodoo_Initialize(Bits emulation_type, Bits card_type, bool max_voodoomem) {
	if ((emulation_type <= 0) || (emulation_type > 2)) return;

	int board = VOODOO_1;
	
	switch (card_type) {
		case 1:
			if (max_voodoomem) board = VOODOO_1_DTMU;
			else board = VOODOO_1;
			break;
		case 2:
//			if (max_voodoomem) board = VOODOO_2_DTMU;
//			else
				board = VOODOO_2;
			break;
		default:
			E_Exit("invalid voodoo card type specified");
			break;
	}

	voodoo_pagehandler = new Voodoo_PageHandler(0);

	v = new voodoo_state;
	v->ogl = false;
	extern bool OpenGL_using(void);
	if (emulation_type == 2) v->ogl = OpenGL_using();

	LOG(LOG_VOODOO,LOG_DEBUG)("voodoo: ogl=%u",v->ogl);

	vdraw.vfreq = 1000.0f/60.0f;

	voodoo_init(board);
}

void Voodoo_Shut_Down() {
	voodoo_shutdown();

	if (v != NULL) {
		delete v;
		v = NULL;
	}
	if (voodoo_pagehandler != NULL) {
		delete voodoo_pagehandler;
		voodoo_pagehandler = NULL;
	}
}

void Voodoo_PCI_InitEnable(Bitu val) {
	v->pci.init_enable = (UINT32)val;
}

void Voodoo_PCI_Enable(bool enable) {
	v->clock_enabled = enable;
	CPU_Core_Dyn_X86_SaveDHFPUState();
	Voodoo_UpdateScreenStart();
	CPU_Core_Dyn_X86_RestoreDHFPUState();
}

PageHandler* Voodoo_GetPageHandler() {
	return voodoo_pagehandler;
}

