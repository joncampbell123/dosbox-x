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

extern uint8_t  * lookupRMregb[];
extern uint16_t * lookupRMregw[];
extern Bit32u * lookupRMregd[];
extern uint8_t  * lookupRMEAregb[256];
extern uint16_t * lookupRMEAregw[256];
extern Bit32u * lookupRMEAregd[256];

#define GetRM												\
	uint8_t rm=Fetchb();

#define Getrb												\
	uint8_t * rmrb;											\
	rmrb=lookupRMregb[rm];			
	
#define Getrw												\
	uint16_t * rmrw;											\
	rmrw=lookupRMregw[rm];			

#define Getrd												\
	Bit32u * rmrd;											\
	rmrd=lookupRMregd[rm];			


#define GetRMrb												\
	GetRM;													\
	Getrb;													

#define GetRMrw												\
	GetRM;													\
	Getrw;													

#define GetRMrd												\
	GetRM;													\
	Getrd;													


#define GetEArb												\
	uint8_t * earb=lookupRMEAregb[rm];

#define GetEArw												\
	uint16_t * earw=lookupRMEAregw[rm];

#define GetEArd												\
	Bit32u * eard=lookupRMEAregd[rm];


