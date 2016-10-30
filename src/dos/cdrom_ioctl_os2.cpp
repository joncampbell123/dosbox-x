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
#include "dosbox.h"
#include "cdrom.h"

#if defined (OS2)
#define INCL_DOSFILEMGR
#define INCL_DOSERRORS
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#include "os2.h"

// Ripped from linux/cdrom.h
#define CD_FRAMESIZE_RAW 2352
#define CD_FRAMESIZE 2048

CDROM_Interface_Ioctl::CDROM_Interface_Ioctl(void) : CDROM_Interface_SDL(){
    strcpy(device_name, "");
}

bool CDROM_Interface_Ioctl::GetUPC(unsigned char& attr, char* upc){
    HFILE cdrom_fd = 0;
    ULONG ulAction = 0;
    APIRET rc = DosOpen((unsigned char*)device_name, &cdrom_fd, &ulAction, 0L, FILE_NORMAL, OPEN_ACTION_OPEN_IF_EXISTS,
            OPEN_FLAGS_DASD | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY, 0L);
    if (rc != NO_ERROR) {
        return false;
    }
    char data[50];
    ULONG len = sizeof(data);
    char sig[] = {'C', 'D', '0', '1'};
    ULONG sigsize = 4;
    rc = DosDevIOCtl(cdrom_fd, IOCTL_CDROMDISK, CDROMDISK_GETUPC, sig, sigsize, &sigsize,
                 data, len, &len);
    if (rc != NO_ERROR) {
        return false;
    }
    rc = DosClose(cdrom_fd);
    return rc == NO_ERROR;
}

bool CDROM_Interface_Ioctl::ReadSectors(PhysPt buffer, bool raw, unsigned long sector, unsigned long num){
    HFILE cdrom_fd = 0;
    ULONG ulAction = 0;
    APIRET rc = DosOpen((unsigned char*)device_name, &cdrom_fd, &ulAction, 0L, FILE_NORMAL, OPEN_ACTION_OPEN_IF_EXISTS,
            OPEN_FLAGS_DASD | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY, 0L);
    if (rc != NO_ERROR) {
        return false;
    }

    Bitu buflen = raw ? num * CD_FRAMESIZE_RAW : num * CD_FRAMESIZE;
    Bit8u* buf = new Bit8u[buflen];
    int ret = NO_ERROR;

    if (raw) {
    	struct paramseek {
       		UCHAR sig[4];
       		UCHAR mode;
      		ULONG sec;
       		
       		paramseek(ULONG sector)
       		{
       			sig[0] = 'C'; sig[1] = 'D'; sig[2] = '0'; sig[3] = '1';
       			sec = sector;
       		}
    	} param_seek(sector);
       	ULONG paramsize = sizeof (paramseek);
		rc = DosDevIOCtl(cdrom_fd, IOCTL_CDROMDISK, CDROMDISK_SEEK, &param_seek, paramsize, &paramsize,
                 0, 0, 0);
        if (rc != NO_ERROR) {
            return false;
        }

		struct paramread {
			UCHAR sig[4];
			UCHAR mode;
			USHORT number;
			BYTE sec;
       		BYTE reserved;
       		BYTE interleave;
       		
       		paramread(USHORT num)
       		{
       			sig[0] = 'C'; sig[1] = 'D'; sig[2] = '0'; sig[3] = '1';
       			mode = 0; number = num;
       			sec = interleave = 0;
       		}
       	} param_read(num);
       	paramsize = sizeof (paramread);
		ULONG len = buflen;
		rc = DosDevIOCtl(cdrom_fd, IOCTL_CDROMDISK, CDROMDISK_READLONG, &param_read, paramsize, &paramsize,
                 buf, len, &len);
        if (rc != NO_ERROR) {
            return false;
        }
    } else {
        ULONG pos = 0;
        rc = DosSetFilePtr(cdrom_fd, sector * CD_FRAMESIZE, FILE_BEGIN, &pos);
        if (rc != NO_ERROR) {
            return false;
        }
        ULONG read = 0;
        rc = DosRead(cdrom_fd, buf, buflen, &read);
        if (rc != NO_ERROR || read != buflen) {
            return false;
        }
    }
    rc = DosClose(cdrom_fd);
    MEM_BlockWrite(buffer, buf, buflen);
    delete[] buf;

    return (ret == NO_ERROR);
}

bool CDROM_Interface_Ioctl::SetDevice(char* path, int forceCD) {
    bool success = CDROM_Interface_SDL::SetDevice(path, forceCD);

    if (success) {
		char temp[3] = {0, 0,  0};
		if (path[1] == ':') {
			temp[0] = path[0];
			temp[1] = path[1];
			temp[2] = 0;
		}
        strncpy(device_name, temp, 512);
    } else {
            strcpy(device_name, "");
            success = false;
    }

    return success;
}

bool CDROM_Interface_Ioctl::ReadSectorsHost(void *buffer, bool raw, unsigned long sector, unsigned long num)
{
	return false;/*TODO*/
};

#endif
