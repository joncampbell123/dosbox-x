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


#ifndef DOSBOX_VOODOO_EMU_H
#define DOSBOX_VOODOO_EMU_H

#include <stdlib.h>

#include "dosbox.h"

#include "voodoo_types.h"
#include "voodoo_data.h"


extern voodoo_state *v;

void voodoo_w(UINT32 offset, UINT32 data, UINT32 mask);
UINT32 voodoo_r(UINT32 offset);

void voodoo_init(int type);
void voodoo_shutdown();
void voodoo_leave(void);

void voodoo_activate(void);
void voodoo_update_dimensions(void);
void voodoo_set_window(void);

void voodoo_vblank_flush(void);
void voodoo_swap_buffers(voodoo_state *v);


extern void Voodoo_UpdateScreenStart();
extern void Voodoo_Output_Enable(bool enabled);
extern bool Voodoo_GetRetrace();
extern double Voodoo_GetVRetracePosition();
extern double Voodoo_GetHRetracePosition();


extern void CPU_Core_Dyn_X86_SaveDHFPUState(void);
extern void CPU_Core_Dyn_X86_RestoreDHFPUState(void);

#endif
