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

#define SaveIP() reg_eip=(uint32_t)(inst.cseip-SegBase(cs));
#define LoadIP() inst.cseip=SegBase(cs)+reg_eip;
#define GetIP()	(inst.cseip-SegBase(cs))

#define RunException() {										\
	CPU_Exception(cpu.exception.which,cpu.exception.error);		\
	continue;													\
}

static INLINE uint8_t the_Fetchb(EAPoint & loc) {
	uint8_t temp=LoadMb(loc);
	loc+=1;
	return temp;
}
	
static INLINE uint16_t the_Fetchw(EAPoint & loc) {
	uint16_t temp=LoadMw(loc);
	loc+=2;
	return temp;
}
static INLINE uint32_t the_Fetchd(EAPoint & loc) {
	uint32_t temp=LoadMd(loc);
	loc+=4;
	return temp;
}

#define Fetchb() the_Fetchb(inst.cseip)
#define Fetchw() the_Fetchw(inst.cseip)
#define Fetchd() the_Fetchd(inst.cseip)

#define Fetchbs() (int8_t)the_Fetchb(inst.cseip)
#define Fetchws() (int16_t)the_Fetchw(inst.cseip)
#define Fetchds() (int32_t)the_Fetchd(inst.cseip)

#define Push_16 CPU_Push16
#define Push_32 CPU_Push32
#define Pop_16 CPU_Pop16
#define Pop_32 CPU_Pop32

