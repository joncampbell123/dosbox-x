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


#ifndef DOSBOX_VOODOO_IF_H
#define DOSBOX_VOODOO_IF_H

typedef struct vdraw {
	Bitu width;
	Bitu height;
	Bitu bpp;
	float vfreq;
	double frame_start;
	bool doublewidth;
	bool doubleheight;

	bool override_on;

	bool screen_update_requested;
	bool screen_update_pending;
} voodoo_draw;


class Voodoo_PageHandler : public PageHandler {
public:
	Voodoo_PageHandler(HostPt /*addr*/){
		flags=PFLAG_NOCODE;
	}

	~Voodoo_PageHandler() {
	}

	uint8_t readb(PhysPt addr);
	void writeb(PhysPt addr,uint8_t val);
	Bit16u readw(PhysPt addr);
	void writew(PhysPt addr,Bit16u val);
	Bit32u readd(PhysPt addr);
	void writed(PhysPt addr,Bit32u val);
};


void Voodoo_Initialize(Bits emulation_type, Bits card_type, bool max_voodoomem);
void Voodoo_Shut_Down();

void Voodoo_PCI_InitEnable(Bitu val);
void Voodoo_PCI_Enable(bool enable);

PageHandler* Voodoo_GetPageHandler();

#endif
