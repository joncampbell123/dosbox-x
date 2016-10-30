/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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
 

#include <string.h>
#include "cdrom.h"
#include "support.h"

#if defined (LINUX)
#include <fcntl.h>
#include <unistd.h>
#include <linux/cdrom.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

CDROM_Interface_Ioctl::CDROM_Interface_Ioctl(void) : CDROM_Interface_SDL()
{
	strcpy(device_name, "");
}

bool CDROM_Interface_Ioctl::GetUPC(unsigned char& attr, char* upc)
{
	int cdrom_fd = open(device_name, O_RDONLY | O_NONBLOCK);
	if (cdrom_fd == -1) return false;
	
	struct cdrom_mcn cdrom_mcn;
	int ret = ioctl(cdrom_fd, CDROM_GET_MCN, &cdrom_mcn);
	
	close(cdrom_fd);
	
	if (ret > 0) {
		attr = 0;
		safe_strncpy(upc, (char*)cdrom_mcn.medium_catalog_number, 14);
	}

	return (ret > 0);
}

bool CDROM_Interface_Ioctl::ReadSectors(PhysPt buffer, bool raw, unsigned long sector, unsigned long num)
{
	int cdrom_fd = open(device_name, O_RDONLY | O_NONBLOCK);
	if (cdrom_fd == -1) return false;
	
	Bits buflen = raw ? num * CD_FRAMESIZE_RAW : num * CD_FRAMESIZE;
	Bit8u* buf = new Bit8u[buflen];	
	int ret;
	
	if (raw) {
		struct cdrom_read cdrom_read;
		cdrom_read.cdread_lba = sector;
		cdrom_read.cdread_bufaddr = (char*)buf;
		cdrom_read.cdread_buflen = buflen;
		
		ret = ioctl(cdrom_fd, CDROMREADRAW, &cdrom_read);		
	} else {
		ret = lseek(cdrom_fd, sector * CD_FRAMESIZE, SEEK_SET);
		if (ret >= 0) ret = read(cdrom_fd, buf, buflen);
		if (ret != buflen) ret = -1;
	}
	close(cdrom_fd);

	MEM_BlockWrite(buffer, buf, buflen);
	delete[] buf;
	
	return (ret > 0);
}

bool CDROM_Interface_Ioctl::SetDevice(char* path, int forceCD)
{
	bool success = CDROM_Interface_SDL::SetDevice(path, forceCD);
	
	if (success) {
		const char* tmp = SDL_CDName(forceCD);
		if (tmp) safe_strncpy(device_name, tmp, 512);
		else success = false;
	}
	
	return success;
}

bool CDROM_Interface_Ioctl::ReadSectorsHost(void *buffer, bool raw, unsigned long sector, unsigned long num)
{
	return false;/*TODO*/
};

#endif
