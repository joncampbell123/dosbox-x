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

#ifndef __XMS_H__
#define __XMS_H__

Bitu	XMS_QueryFreeMemory		(uint16_t& largestFree, uint16_t& totalFree);
Bitu	XMS_AllocateMemory		(Bitu size, uint16_t& handle);
Bitu	XMS_FreeMemory			(Bitu handle);
Bitu	XMS_MoveMemory			(PhysPt bpt);
Bitu	XMS_LockMemory			(Bitu handle, uint32_t& address);
Bitu	XMS_UnlockMemory		(Bitu handle);
Bitu	XMS_GetHandleInformation(Bitu handle, uint8_t& lockCount, uint8_t& numFree, uint32_t& size);
Bitu	XMS_ResizeMemory		(Bitu handle, Bitu newSize);

Bitu	XMS_EnableA20			(bool enable);
Bitu	XMS_GetEnabledA20		(void);

#endif
