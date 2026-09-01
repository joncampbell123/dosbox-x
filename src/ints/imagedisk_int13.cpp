/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include <assert.h>

#include "dosbox.h"
#include "callback.h"
#include "bios.h"
#include "regs.h"
#include "mem.h"
#include "dos_inc.h"
#include "cpu.h"
#include "bios_disk.h"
#include "imagedisk_int13.h"

#if !defined(OSFREE)
extern Bitu call_int13;

unsigned int INT13Xfer = 0;
size_t INT13XferSize = 4096;

static void imageDiskCallINT13(void) {
	unsigned int rv = CALLBACK_RealPointer(call_int13);
	Bitu oldIF=GETFLAG(IF);
	SETFLAGBIT(IF,true);
	uint16_t oldcs=SegValue(cs);
	uint32_t oldeip=reg_eip;
	SegSet16(cs,rv>>16);
	reg_eip=(rv&0xFFFF)+4+5;
	DOSBOX_RunMachine();
	reg_eip=oldeip;
	SegSet16(cs,oldcs);
	SETFLAGBIT(IF,oldIF);
}

Int13Status imageDiskINT13Drive::Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size) {
	if (!enable_int13 || busy) return subdisk->Read_Sector(head,cylinder,sector,data,req_sector_size);

	Int13Status ret = Int13Status::ResetFailed;
	unsigned int retry = 3;

	if (req_sector_size == 0) req_sector_size = sector_size;

	//LOG_MSG("INT13 read C/H/S %u/%u/%u busy=%u",cylinder,head,sector,busy);

	if (!busy && sector_size == req_sector_size && sector_size <= INT13XferSize) {
		busy = true;

		if (INT13Xfer == 0) INT13Xfer = DOS_GetMemory(INT13XferSize/16u,"INT 13 transfer buffer");

		unsigned int s_eax = reg_eax;
		unsigned int s_ebx = reg_ebx;
		unsigned int s_ecx = reg_ecx;
		unsigned int s_edx = reg_edx;
		unsigned int s_esi = reg_esi;
		unsigned int s_edi = reg_edi;
		unsigned int s_esp = reg_esp;
		unsigned int s_ebp = reg_ebp;
		unsigned int s_es  = SegValue(es);
		unsigned int s_fl  = reg_flags;

again:
		reg_eax = 0x200/*read command*/ | 1/*count*/;
		reg_ebx = 0;
		reg_ch = cylinder;
		reg_cl = sector;
		reg_dh = head;
		reg_dl = bios_disk;
		CPU_SetSegGeneral(es,INT13Xfer);

		imageDiskCallINT13();

		if (reg_flags & FLAG_CF) {
			ret = static_cast<Int13Status>(reg_ah);
			if (ret == Int13Status::NoError) ret = Int13Status::ResetFailed;

			if (ret == Int13Status::DiskChanged) {
				diskChangeFlag = true;
				if (--retry > 0) goto again;
			}
		}
		else {
			ret = Int13Status::NoError;
			MEM_BlockRead32(INT13Xfer<<4,data,sector_size);
			data = (void*)((char*)data + sector_size);
			if ((++sector) >= (sectors + 1)) {
				assert(sector == (sectors + 1));
				sector = 1;
				if ((++head) >= heads) {
					assert(head == heads);
					head = 0;
					cylinder++;
				}
			}
		}

		reg_eax = s_eax;
		reg_ebx = s_ebx;
		reg_ecx = s_ecx;
		reg_edx = s_edx;
		reg_esi = s_esi;
		reg_edi = s_edi;
		reg_esp = s_esp;
		reg_ebp = s_ebp;
		reg_flags = s_fl;
		CPU_SetSegGeneral(es,s_es);

		busy = false;
	}

	return ret;
}

Int13Status imageDiskINT13Drive::Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size) {
	if (INT13Xfer == 0) INT13Xfer = DOS_GetMemory(INT13XferSize/16u,"INT 13 transfer buffer");

	return subdisk->Write_Sector(head,cylinder,sector,data,req_sector_size);
}

Int13Status imageDiskINT13Drive::Read_AbsoluteSector(uint32_t sectnum, void * data) {
	unsigned int c,h,s;

	if (sectors == 0 || heads == 0)
		return Int13Status::SectorNotFound;

	s = (sectnum % sectors) + 1;
	h = (sectnum / sectors) % heads;
	c = (sectnum / sectors / heads);
	return Read_Sector(h,c,s,data);
}

Int13Status imageDiskINT13Drive::Write_AbsoluteSector(uint32_t sectnum, const void * data) {
	unsigned int c,h,s;

	if (sectors == 0 || heads == 0)
		return Int13Status::SectorNotFound;

	s = (sectnum % sectors) + 1;
	h = (sectnum / sectors) % heads;
	c = (sectnum / sectors / heads);
	return Write_Sector(h,c,s,data);
}

void imageDiskINT13Drive::UpdateFloppyType(void) {
	subdisk->UpdateFloppyType();
}

void imageDiskINT13Drive::Set_Reserved_Cylinders(Bitu resCyl) {
	subdisk->Set_Reserved_Cylinders(resCyl);
}

uint32_t imageDiskINT13Drive::Get_Reserved_Cylinders() {
	return subdisk->Get_Reserved_Cylinders();
}

void imageDiskINT13Drive::Set_Geometry(uint32_t setHeads, uint32_t setCyl, uint32_t setSect, uint32_t setSectSize) {
	heads = setHeads;
	cylinders = setCyl;
	sectors = setSect;
	sector_size = setSectSize;
	return subdisk->Set_Geometry(setHeads,setCyl,setSect,setSectSize);
}

void imageDiskINT13Drive::Get_Geometry(uint32_t * getHeads, uint32_t *getCyl, uint32_t *getSect, uint32_t *getSectSize) {
	return subdisk->Get_Geometry(getHeads,getCyl,getSect,getSectSize);
}

uint8_t imageDiskINT13Drive::GetBiosType(void) {
	return subdisk->GetBiosType();
}

uint32_t imageDiskINT13Drive::getSectSize(void) {
	return subdisk->getSectSize();
}

bool imageDiskINT13Drive::detectDiskChange(void) {
	if (enable_int13 && !busy) {
		busy = true;

		unsigned int s_eax = reg_eax;
		unsigned int s_ebx = reg_ebx;
		unsigned int s_ecx = reg_ecx;
		unsigned int s_edx = reg_edx;
		unsigned int s_esi = reg_esi;
		unsigned int s_edi = reg_edi;
		unsigned int s_esp = reg_esp;
		unsigned int s_ebp = reg_ebp;
		unsigned int s_fl  = reg_flags;

		reg_eax = 0x1600/*disk change detect*/;
		reg_dl = bios_disk;
		CPU_SetSegGeneral(es,INT13Xfer);

		imageDiskCallINT13();

		if (reg_flags & FLAG_CF) {
			if (reg_ah == 0x06) {
				LOG(LOG_MISC,LOG_DEBUG)("INT13 image disk change flag");
				diskChangeFlag = true;
			}
		}

		reg_eax = s_eax;
		reg_ebx = s_ebx;
		reg_ecx = s_ecx;
		reg_edx = s_edx;
		reg_esi = s_esi;
		reg_edi = s_edi;
		reg_esp = s_esp;
		reg_ebp = s_ebp;
		reg_flags = s_fl;

		busy = false;
	}

	return imageDisk::detectDiskChange();
}

imageDiskINT13Drive::imageDiskINT13Drive(imageDisk *sdisk) : imageDisk(ID_INT13) {
	subdisk = sdisk;
	subdisk->Addref();

	drvnum         = subdisk->drvnum;
	diskname       = subdisk->diskname;
	active         = subdisk->active;
	sector_size    = subdisk->sector_size;
	heads          = subdisk->heads;
	cylinders      = subdisk->cylinders;
	sectors        = subdisk->sectors;
	hardDrive      = subdisk->hardDrive;
	diskSizeK      = subdisk->diskSizeK;
	diskChangeFlag = subdisk->diskChangeFlag;
}

imageDiskINT13Drive::~imageDiskINT13Drive() {
	subdisk->Release();
}
#endif
