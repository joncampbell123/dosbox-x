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

#ifndef DOSBOX_MAPPER_H
#define DOSBOX_MAPPER_H

enum MapKeys {
	MK_f1,MK_f2,MK_f3,MK_f4,MK_f5,MK_f6,MK_f7,MK_f8,MK_f9,MK_f10,MK_f11,MK_f12,
	MK_return,MK_kpminus,MK_scrolllock,MK_printscreen,MK_pause

};

typedef void (MAPPER_Handler)(bool pressed);
void MAPPER_AddHandler(MAPPER_Handler * handler,MapKeys key,Bitu mods,char const * const eventname,char const * const buttonname);
void MAPPER_Init(void);
void MAPPER_StartUp(Section * sec);
void MAPPER_Run(bool pressed);
void MAPPER_RunInternal();
void MAPPER_LosingFocus(void);


#define MMOD1 0x1
#define MMOD2 0x2

#endif
