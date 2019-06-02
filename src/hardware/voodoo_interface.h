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

	Bitu readb(PhysPt addr);
	void writeb(PhysPt addr,Bitu val);
	Bitu readw(PhysPt addr);
	void writew(PhysPt addr,Bitu val);
	Bitu readd(PhysPt addr);
	void writed(PhysPt addr,Bitu val);
};


void Voodoo_Initialize(Bits emulation_type, Bits card_type, bool max_voodoomem);
void Voodoo_Shut_Down();

void Voodoo_PCI_InitEnable(Bitu val);
void Voodoo_PCI_Enable(bool enable);

PageHandler* Voodoo_GetPageHandler();

#endif
