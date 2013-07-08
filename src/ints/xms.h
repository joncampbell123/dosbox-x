/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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

#ifndef __XMS_H__
#define __XMS_H__

Bitu	XMS_QueryFreeMemory		(Bit16u& largestFree, Bit16u& totalFree);
Bitu	XMS_AllocateMemory		(Bitu size, Bit16u& handle);
Bitu	XMS_FreeMemory			(Bitu handle);
Bitu	XMS_MoveMemory			(PhysPt bpt);
Bitu	XMS_LockMemory			(Bitu handle, Bit32u& address);
Bitu	XMS_UnlockMemory		(Bitu handle);
Bitu	XMS_GetHandleInformation(Bitu handle, Bit8u& lockCount, Bit8u& numFree, Bit16u& size);
Bitu	XMS_ResizeMemory		(Bitu handle, Bitu newSize);

Bitu	XMS_EnableA20			(bool enable);
Bitu	XMS_GetEnabledA20		(void);

#endif
