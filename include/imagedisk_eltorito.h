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

#ifndef DOSBOX_IMAGE_DISK_ELTORITO_H
#define DOSBOX_IMAGE_DISK_ELTORITO_H

#include "bios_disk.h"

/* C++ class implementing El Torito floppy emulation */
class imageDiskElToritoFloppy : public imageDisk {
public:
    /* Read_Sector and Write_Sector take care of geometry translation for us,
     * then call the absolute versions. So, we override the absolute versions only */
    Int13Status Read_AbsoluteSector(uint32_t sectnum, void * data) override {
        unsigned char buffer[2048];

	if (!src_drive)
            return Int13Status::ControllerFailure;
        if (!src_drive->ReadSectorsHost(buffer,false,cdrom_sector_offset+(sectnum>>2)/*512 byte/sector to 2048 byte/sector conversion*/,1))
            return Int13Status::ControllerFailure;

        if ((sectnum & 3) * 512 + 512 > sizeof(buffer)) return Int13Status::SectorNotFound;
        memcpy(data,buffer+((sectnum&3)*512),512);
        return Int13Status::NoError;
    }
    Int13Status Write_AbsoluteSector(uint32_t sectnum,const void * data) override {
        (void)sectnum;//UNUSED
        (void)data;//UNUSED
        return Int13Status::WriteProtected; /* fail, read only */
    }
    imageDiskElToritoFloppy(unsigned char new_CDROM_drive,unsigned long new_cdrom_sector_offset,unsigned char floppy_emu_type) : imageDisk((FILE *)NULL,NULL,0,false), CDROM_drive(new_CDROM_drive), cdrom_sector_offset(new_cdrom_sector_offset), floppy_type(floppy_emu_type) {
        diskimg = NULL;
        sector_size = 512;
        class_id = ID_EL_TORITO_FLOPPY;

        bool GetMSCDEXDrive(unsigned char drive_letter,CDROM_Interface **_cdrom);
        GetMSCDEXDrive(CDROM_drive-'A',&src_drive);/*addref src_drive*/

        if (floppy_emu_type == 1) { /* 1.2MB */
            heads = 2;
            cylinders = 80;
            sectors = 15;
        }
        else if (floppy_emu_type == 2) { /* 1.44MB */
            heads = 2;
            cylinders = 80;
            sectors = 18;
        }
        else if (floppy_emu_type == 3) { /* 2.88MB */
            heads = 2;
            cylinders = 80;
            sectors = 36; /* FIXME: right? */
        }
        else {
            heads = 2;
            cylinders = 69;
            sectors = 14;
            LOG_MSG("BUG! unsupported floppy_emu_type in El Torito floppy object\n");
        }

        diskSizeK = ((uint64_t)heads * cylinders * sectors * sector_size) / 1024;
        active = true;
    }
    virtual ~imageDiskElToritoFloppy() {
        if (src_drive) {
            src_drive->Release();
            src_drive = NULL;
        }
    }

    CDROM_Interface *src_drive=NULL;
    unsigned char CDROM_drive;
    unsigned long cdrom_sector_offset;
    unsigned char floppy_type;
/*
    int class_id;

    bool hardDrive;
    bool active;
    FILE *diskimg;
    std::string diskname;
    uint8_t floppytype;

    uint32_t sector_size;
    uint32_t heads,cylinders,sectors;
    uint32_t reserved_cylinders;
    uint64_t current_fpos; */
};

#endif
