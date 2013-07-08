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

/* $Id: inout.h,v 1.13 2009-05-27 09:15:41 qbix79 Exp $ */

#ifndef DOSBOX_INOUT_H
#define DOSBOX_INOUT_H

#define IO_MAX (64*1024+3)

#define IO_MB	0x1
#define IO_MW	0x2
#define IO_MD	0x4
#define IO_MA	(IO_MB | IO_MW | IO_MD )

typedef Bitu IO_ReadHandler(Bitu port,Bitu iolen);
typedef void IO_WriteHandler(Bitu port,Bitu val,Bitu iolen);

extern IO_WriteHandler * io_writehandlers[3][IO_MAX];
extern IO_ReadHandler * io_readhandlers[3][IO_MAX];

void IO_RegisterReadHandler(Bitu port,IO_ReadHandler * handler,Bitu mask,Bitu range=1);
void IO_RegisterWriteHandler(Bitu port,IO_WriteHandler * handler,Bitu mask,Bitu range=1);

void IO_FreeReadHandler(Bitu port,Bitu mask,Bitu range=1);
void IO_FreeWriteHandler(Bitu port,Bitu mask,Bitu range=1);

void IO_WriteB(Bitu port,Bitu val);
void IO_WriteW(Bitu port,Bitu val);
void IO_WriteD(Bitu port,Bitu val);

Bitu IO_ReadB(Bitu port);
Bitu IO_ReadW(Bitu port);
Bitu IO_ReadD(Bitu port);

/* Classes to manage the IO objects created by the various devices.
 * The io objects will remove itself on destruction.*/
class IO_Base{
protected:
	bool installed;
	Bitu m_port, m_mask,m_range;
public:
	IO_Base():installed(false){};
};
class IO_ReadHandleObject: private IO_Base{
public:
	void Install(Bitu port,IO_ReadHandler * handler,Bitu mask,Bitu range=1);
	~IO_ReadHandleObject();
};
class IO_WriteHandleObject: private IO_Base{
public:
	void Install(Bitu port,IO_WriteHandler * handler,Bitu mask,Bitu range=1);
	~IO_WriteHandleObject();
};

static INLINE void IO_Write(Bitu port,Bit8u val) {
	IO_WriteB(port,val);
}
static INLINE Bit8u IO_Read(Bitu port){
	return (Bit8u)IO_ReadB(port);
}

#endif
