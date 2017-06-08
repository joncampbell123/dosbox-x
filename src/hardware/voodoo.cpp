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

#if 0
#include <stdlib.h>
#include <string.h>

#include "dosbox.h"
#include "config.h"
#include "setup.h"
#include "cross.h"
#include "control.h"

#include "paging.h"
#include "mem.h"

#include "voodoo.h"
#include "pci_bus.h"
#include "voodoo_interface.h"

class VOODOO;
static VOODOO* voodoo_dev;

static Bit32u voodoo_current_lfb=(VOODOO_INITIAL_LFB&0xffff0000);


class VOODOO:public Module_base{
private:
	Bits emulation_type;
public:
	VOODOO(Section* configuration):Module_base(configuration){
		emulation_type=-1;

		Section_prop * section=static_cast<Section_prop *>(configuration);
		std::string voodoo_type_str(section->Get_string("voodoo"));
		if (voodoo_type_str=="false") {
			emulation_type=0;
		} else if (voodoo_type_str=="software") {
			emulation_type=1;
#if C_OPENGL
		} else if ((voodoo_type_str=="opengl") || (voodoo_type_str=="auto")) {
			emulation_type=2;
#else
		} else if (voodoo_type_str=="auto") {
			emulation_type=1;
#endif
		} else {
			emulation_type=0;
		}

		Bits card_type = 1;
		bool max_voodoomem = true;

		bool needs_pci_device = false;

		switch (emulation_type) {
			case 1:
			case 2:
				Voodoo_Initialize(emulation_type, card_type, max_voodoomem);
				needs_pci_device = true;
				break;
			default:
				break;
		}

		if (needs_pci_device) PCI_AddSST_Device(card_type);
	}

	~VOODOO(){
		PCI_RemoveSST_Device();

		if (emulation_type<=0) return;

		switch (emulation_type) {
			case 1:
			case 2:
				Voodoo_Shut_Down();
				break;
			default:
				break;
		}

		emulation_type=-1;
	}

	void PCI_InitEnable(Bitu val) {
		switch (emulation_type) {
			case 1:
			case 2:
				Voodoo_PCI_InitEnable(val);
				break;
			default:
				break;
		}
	}

	void PCI_Enable(bool enable) {
		switch (emulation_type) {
			case 1:
			case 2:
				Voodoo_PCI_Enable(enable);
				break;
			default:
				break;
		}
	}

	PageHandler* GetPageHandler() {
		switch (emulation_type) {
			case 1:
			case 2:
				return Voodoo_GetPageHandler();
			default:
				break;
		}
		return NULL;
	}

};


void VOODOO_PCI_InitEnable(Bitu val) {
	if (voodoo_dev!=NULL) {
		voodoo_dev->PCI_InitEnable(val);
	}
}

void VOODOO_PCI_Enable(bool enable) {
	if (voodoo_dev!=NULL) {
		voodoo_dev->PCI_Enable(enable);
	}
}


void VOODOO_PCI_SetLFB(Bit32u lfbaddr) {
	voodoo_current_lfb=(lfbaddr&0xffff0000);
}

bool VOODOO_PCI_CheckLFBPage(Bitu page) {
	if ((page>=(voodoo_current_lfb>>12)) &&
		(page<(voodoo_current_lfb>>12)+VOODOO_PAGES))
		return true;
	return false;
}

PageHandler* VOODOO_GetPageHandler() {
	if (voodoo_dev!=NULL) {
		return voodoo_dev->GetPageHandler();
	}
	return NULL;
}


void VOODOO_Destroy(Section* /*sec*/) {
	delete voodoo_dev;
	voodoo_dev=NULL;
}

void VOODOO_OnPowerOn(Section* /*sec*/) {
	if (voodoo_dev == NULL) {
		voodoo_current_lfb=(VOODOO_INITIAL_LFB&0xffff0000);
		voodoo_dev = new VOODOO(control->GetSection("pci"));
	}
}

void VOODOO_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing Voodoo/3DFX emulation");

	AddExitFunction(AddExitFunctionFuncPair(VOODOO_Destroy),true);
	AddVMEventFunction(VM_EVENT_POWERON,AddVMEventFunctionFuncPair(VOODOO_OnPowerOn));
}
#endif
