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

extern Bit8u  * lookupRMregb[];
extern Bit16u * lookupRMregw[];
extern Bit32u * lookupRMregd[];
extern Bit8u  * lookupRMEAregb[256];
extern Bit16u * lookupRMEAregw[256];
extern Bit32u * lookupRMEAregd[256];

#define GetRM												\
	Bit8u rm=Fetchb();

#define Getrb												\
	Bit8u * rmrb;											\
	rmrb=lookupRMregb[rm];			
	
#define Getrw												\
	Bit16u * rmrw;											\
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
	Bit8u * earb=lookupRMEAregb[rm];

#define GetEArw												\
	Bit16u * earw=lookupRMEAregw[rm];

#define GetEArd												\
	Bit32u * eard=lookupRMEAregd[rm];


