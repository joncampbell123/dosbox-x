/*
 *  Copyright (C) 2026  The DOSBox-X Team
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

#include <array>
#include <cstdint>

#include "dosbox.h"

#include "bios.h"
#include "bios_disk.h"
#include "callback.h"
#include "cpu.h"
#include "dos/drives.h"
#include "ide.h"
#include "int13.h"
#include "mem.h"
#include "regs.h"

extern bool int13_disk_change_detect_enable;
extern bool int13_extensions_enable;
extern bool int13_enable_48bitLBA;
extern PhysPt floppyparm0;

static Bitu call_int13 = 0;
static Int13Status last_status;
static uint8_t last_drive;
static bool killRead;

static uint8_t GetDosDriveNumber(uint8_t biosNum) {
    switch(biosNum) {
        case 0x0:
            return 0x0;
        case 0x1:
            return 0x1;
        case 0x80:
            return 0x2;
        case 0x81:
            return 0x3;
        case 0x82:
            return 0x4;
        case 0x83:
            return 0x5;
        default:
            return 0x7f;
    }
}

static bool driveInactive(uint8_t driveNum) {
    if(driveNum>=MAX_DISK_IMAGES) {
        LOG(LOG_BIOS,LOG_DEBUG)("INT 13h: BIOS drive 0x%02x does not exist", reg_dl);
        last_status = Int13Status::BadCommand;
        CALLBACK_SCF(true);
        return true;
    }
    if(imageDiskList[driveNum] == NULL) {
        LOG(LOG_BIOS,LOG_DEBUG)("INT 13h: BIOS drive 0x%02x is not active", reg_dl);
        last_status = Int13Status::BadCommand;
        CALLBACK_SCF(true);
        return true;
    }
    if(!imageDiskList[driveNum]->active) {
        LOG(LOG_BIOS,LOG_DEBUG)("INT 13h: BIOS drive 0x%02x is not active", reg_dl);
        last_status = Int13Status::BadCommand;
        CALLBACK_SCF(true);
        return true;
    }
    return false;
}

struct DiskAddressPacket
{
    uint8_t  size = 0;
    uint8_t  reserved = 0;
    uint16_t sector_count = 0;
    uint16_t buffer_offset = 0;
    uint16_t buffer_segment = 0;
    uint32_t sector = 0;
};

static bool ReadDiskAddressPacket(uint16_t segment, uint16_t offset, DiskAddressPacket &packet)
{
    packet.size = real_readb(segment, offset++);
    packet.reserved = real_readb(segment, offset++);
    packet.sector_count = real_readw(segment, offset); offset += 2;
    packet.buffer_offset = real_readw(segment, offset); offset += 2;
    packet.buffer_segment = real_readw(segment, offset); offset += 2;
    packet.sector = real_readd(segment, offset); offset += 4;

    // The 0x18-byte form uses a flat buffer pointer, which INT 13h does not
    // currently support. Do not silently use its legacy segment:offset fields.
    if (packet.size != 0x10 || packet.reserved != 0) {
        LOG(LOG_BIOS,LOG_WARN)("INT 13h: Invalid disk address packet size %u or reserved byte %u",
                               packet.size,
                               packet.reserved);
        return false;
    }

    /* Although sector addressing is 64-bit, the 32-bit 2 TB limit is sufficient. */
    if (real_readd(segment, offset)) {
        LOG(LOG_BIOS,LOG_WARN)("INT 13h: 64-bit sector addressing is not supported");
        return false;
    }

    return true;
}

void IDE_ResetDiskByBIOS(unsigned char disk);
void IDE_EmuINT13DiskReadByBIOS(unsigned char disk,unsigned int cyl,unsigned int head,unsigned sect);
bool IDE_GetPhysGeometry(unsigned char disk,uint32_t &heads,uint32_t &cyl,uint32_t &sect,uint32_t &size, uint64_t &LBA);
void IDE_EmuINT13DiskReadByBIOS_LBA(unsigned char disk,uint64_t lba);

void diskio_delay(Bits value/*bytes*/, int type = -1);

/* For El Torito "No emulation" INT 13 services */
unsigned char INT13_ElTorito_NoEmuDriveNumber = 0;
CDROM_Interface * INT13_ElTorito_cdrom = NULL;
signed char INT13_ElTorito_IDEInterface = -1; /* (controller * 2) + (is_slave?1:0) */
char INT13_ElTorito_NoEmuCDROMDrive = 0;

bool GetMSCDEXDrive(unsigned char drive_letter, CDROM_Interface **_cdrom);

/* Bytes/sector for a floppy transfer as a real BIOS would use it: taken from the disk
 * base table pointed to by INT 1Eh (offset +3, sector-size code 0=128,1=256,2=512,3=1024,...)
 * rather than assuming 512. A guest may reprogram INT 1Eh to access non-512 media. */
static uint32_t floppyDPT_sector_size(void) {
    RealPt dpt = RealGetVec(0x1E);
    if (dpt) {
        uint8_t code = real_readb(RealSeg(dpt), RealOff(dpt) + 3);
        if (code <= 6) return 128u << code;
    }
    return 512;
}

constexpr auto int13_max_sector_size = 128u << 6u;
constexpr auto int13_edd_sector_size = 512u;
constexpr auto int13_cdrom_sector_size = 2048u;

static void INT13_SetTransferredSectorCount(uint16_t dap_segment,
                                             uint16_t dap_offset,
                                             uint16_t sector_count)
{
    // RBIL specifies that failed EDD reads report the completed block count here.
    real_writew(dap_segment, dap_offset + 2, sector_count);
}

static Bitu INT13_ReadElToritoSectors(const DiskAddressPacket &packet,
                                      uint16_t dap_segment,
                                      uint16_t dap_offset)
{
    if (!INT13_ElTorito_cdrom) {
        last_status = Int13Status::BadCommand;
        reg_ah = 0x01;
        CALLBACK_SCF(true);
        return CBRET_NONE;
    }

    auto *const cdrom = INT13_ElTorito_cdrom;
    std::array<uint8_t, int13_cdrom_sector_size> sector_buffer = {};
    auto buffer_offset = packet.buffer_offset;

    for (uint32_t sector_index = 0; sector_index < packet.sector_count; ++sector_index) {
        diskio_delay(int13_edd_sector_size);
        if (killRead || !cdrom->ReadSectorsHost(sector_buffer.data(),
                                                false,
                                                packet.sector + sector_index,
                                                1)) {
            INT13_SetTransferredSectorCount(dap_segment,
                                            dap_offset,
                                            static_cast<uint16_t>(sector_index));
            killRead = false;
            last_status = Int13Status::SectorNotFound;
            LOG(LOG_BIOS,LOG_DEBUG)("INT 13h: El Torito read failed at LBA %u", packet.sector + sector_index);
            reg_ah = static_cast<uint8_t>(last_status);
            CALLBACK_SCF(true);
            cdrom->Release();
            return CBRET_NONE;
        }

        for (const auto byte : sector_buffer)
            real_writeb(packet.buffer_segment, buffer_offset++, byte);
    }

    last_status = Int13Status::NoError;
    reg_ah = 0x00;
    CALLBACK_SCF(false);
    return CBRET_NONE;
}

static Bitu INT13_ExtendedReadSectors(uint8_t drivenum, bool any_images)
{
    const auto dap_segment = SegValue(ds);
    const auto dap_offset = reg_si;
    DiskAddressPacket packet = {};
    if (!ReadDiskAddressPacket(dap_segment, dap_offset, packet) ||
        packet.sector_count == 0) {
        last_status = Int13Status::BadCommand;
        reg_ah = 0x01;
        CALLBACK_SCF(true);
        return CBRET_NONE;
    }

    if (INT13_ElTorito_NoEmuDriveNumber != 0 &&
        INT13_ElTorito_NoEmuDriveNumber == reg_dl)
        return INT13_ReadElToritoSectors(packet, dap_segment, dap_offset);

    if (!any_images) {
        // Inherit the Earth cdrom uses this as a disk test.
        if ((reg_dl & 0x80) && reg_dh == 0 && (reg_cl & 0x3f) == 1) {
            last_status = Int13Status::NoError;
            reg_ah = 0;
            CALLBACK_SCF(false);
            return CBRET_NONE;
        }
    }

    if (driveInactive(drivenum)) {
        reg_ah = 0xff;
        CALLBACK_SCF(true);
        return CBRET_NONE;
    }

    auto *const disk = imageDiskList[drivenum];
    std::array<uint8_t, int13_max_sector_size> sector_buffer = {};
    if (disk->getSectSize() > sector_buffer.size()) {
        LOG(LOG_BIOS,LOG_WARN)("INT 13h: Extended read failed because drive 0x%02x has %u-byte sectors",
                               reg_dl,
                               disk->getSectSize());
        last_status = Int13Status::Timeout;
        reg_ah = static_cast<uint8_t>(last_status);
        CALLBACK_SCF(true);
        return CBRET_NONE;
    }

    auto buffer_offset = packet.buffer_offset;
    for (uint32_t sector_index = 0; sector_index < packet.sector_count; ++sector_index) {
        sector_buffer.fill(0);
        last_status = disk->Read_AbsoluteSector(packet.sector + sector_index,
                                                sector_buffer.data());

        if (drivenum < 2) diskio_delay(int13_edd_sector_size, 0); // Floppy
        else              diskio_delay(int13_edd_sector_size);

        IDE_EmuINT13DiskReadByBIOS_LBA(reg_dl, packet.sector + sector_index);

        const auto data_transferred = last_status == Int13Status::NoError ||
                                      last_status == Int13Status::DataError;
        if (!killRead && data_transferred) {
            for (auto byte = sector_buffer.cbegin();
                 byte != sector_buffer.cbegin() + int13_edd_sector_size;
                 ++byte)
                real_writeb(packet.buffer_segment, buffer_offset++, *byte);
        }

        if (last_status != Int13Status::NoError || killRead) {
            INT13_SetTransferredSectorCount(
                dap_segment,
                dap_offset,
                static_cast<uint16_t>(sector_index + (data_transferred && !killRead))
            );
            killRead = false;
            if (last_status == Int13Status::NoError) last_status = Int13Status::SectorNotFound;
            LOG(LOG_BIOS,LOG_DEBUG)("INT 13h: Extended read failed on drive 0x%02x at LBA %u (status 0x%02x)",
                                    reg_dl,
                                    packet.sector + sector_index,
                                    static_cast<uint8_t>(last_status));
            reg_ah = static_cast<uint8_t>(last_status);
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
    }

    last_status = Int13Status::NoError;
    reg_ah = 0x00;
    CALLBACK_SCF(false);
    return CBRET_NONE;
}

static Bitu INT13_ReadSectors(uint8_t drivenum)
{
    if (reg_al == 0) {
        last_status = Int13Status::BadCommand;
        reg_ah = 0x01;
        CALLBACK_SCF(true);
        return CBRET_NONE;
    }

    if (drivenum >= MAX_DISK_IMAGES || imageDiskList[drivenum] == NULL) {
        if (drivenum >= DOS_DRIVES || !Drives[drivenum] || Drives[drivenum]->isRemovable()) {
            last_status = Int13Status::BadCommand;
            reg_ah = 0x01;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        // Inherit the Earth cdrom and Amberstar use this as a disk test.
        if ((reg_dl & 0x80) && reg_dh == 0 && (reg_cl & 0x3f) == 1) {
            if (reg_ch == 0) {
                // Write MBR data for the Amberstar installer.
                real_writeb(SegValue(es), reg_bx + 0x1be, 0x80);
                real_writeb(SegValue(es), reg_bx + 0x1c2, 0x06);
            }
            last_status = Int13Status::NoError;
            reg_ah = 0;
            CALLBACK_SCF(false);
            return CBRET_NONE;
        }
    }

    if (driveInactive(drivenum)) {
        reg_ah = 0xff;
        CALLBACK_SCF(true);
        return CBRET_NONE;
    }

    auto *const disk = imageDiskList[drivenum];
    std::array<uint8_t, int13_max_sector_size> sector_buffer = {};
    if (disk->getSectSize() > sector_buffer.size()) {
        LOG(LOG_BIOS,LOG_WARN)("INT 13h: Read failed because drive 0x%02x has %u-byte sectors",
                               reg_dl,
                               disk->getSectSize());
        imageDiskChange[drivenum] = false;
        last_status = Int13Status::Timeout;
        reg_ah = 0x80; /* timeout */
        CALLBACK_SCF(true);
        return CBRET_NONE;
    }

    /* If the disk changed, the first INT 13h read signals an error and sets AH = 06h. */
    if (drivenum < 2 && imageDiskChange[drivenum]) {
        LOG(LOG_BIOS,LOG_DEBUG)("INT 13h: Failing first read of drive 0x%02x to indicate a disk change", reg_dl);
        imageDiskChange[drivenum] = false;
        last_status = Int13Status::DiskChanged;
        reg_ah = 0x06;
        CALLBACK_SCF(true);
        return CBRET_NONE;
    }

    const auto is_hard_drive = (reg_dl & 0x80) != 0;
    const auto cylinder = is_hard_drive ? static_cast<uint32_t>(reg_ch | ((reg_cl & 0xc0) << 2))
                                        : static_cast<uint32_t>(reg_ch);
    const auto first_sector = is_hard_drive ? static_cast<uint32_t>(reg_cl & 0x3f)
                                            : static_cast<uint32_t>(reg_cl);
    const auto transfer_size = is_hard_drive ? 512u : floppyDPT_sector_size();
    const auto buffer_segment = SegValue(es);
    auto buffer_offset = reg_bx;

    for (Bitu sector_index = 0; sector_index < reg_al; ++sector_index) {
        // A DPT sector may be smaller or larger than the mounted image sector.
        // Zeroing first ensures that larger guest transfers are padded.
        sector_buffer.fill(0);
        last_status = disk->Read_Sector(
            static_cast<uint32_t>(reg_dh),
            cylinder,
            first_sector + sector_index,
            sector_buffer.data()
        );

        if (disk->class_id == imageDisk::ID_EL_TORITO_FLOPPY) diskio_delay(512);
        else if (drivenum < 2) diskio_delay(512, 0); // Floppy
        else diskio_delay(512);

        /* IDE emulation: simulate the state change after a real BIOS INT 13h read. */
        IDE_EmuINT13DiskReadByBIOS(
            reg_dl,
            cylinder,
            static_cast<uint32_t>(reg_dh),
            first_sector + sector_index
        );

        /* Real controllers transfer data for CRC-error sectors. */
        if (!killRead && (last_status == Int13Status::NoError ||
                          last_status == Int13Status::DataError)) {
            for (uint32_t byte_index = 0; byte_index < transfer_size; ++byte_index)
                real_writeb(buffer_segment, buffer_offset++, sector_buffer[byte_index]);
        }

        if (last_status != Int13Status::NoError || killRead) {
            killRead = false;
            if (last_status == Int13Status::NoError) last_status = Int13Status::SectorNotFound;
            LOG(LOG_BIOS,LOG_DEBUG)("INT 13h: Read failed on drive 0x%02x at C/H/S %u/%u/%u (status 0x%02x)",
                                    reg_dl,
                                    cylinder,
                                    static_cast<uint32_t>(reg_dh),
                                    static_cast<uint32_t>(first_sector + sector_index),
                                    static_cast<uint8_t>(last_status));
            reg_ah = static_cast<uint8_t>(last_status);
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
    }

    last_status = Int13Status::NoError;
    reg_ah = 0x00;
    CALLBACK_SCF(false);
    return CBRET_NONE;
}

static Bitu INT13_DiskHandler(void) {
    uint16_t segat, bufptr;
    uint8_t sectbuf[2048/*CD-ROM support*/];
    uint8_t  drivenum;
    Bitu  i,t;
    uint32_t rd_cyl = 0, rd_sect = 0, secsz = 512;
    uint64_t LBA = 0;
    last_drive = reg_dl;
    drivenum = GetDosDriveNumber(reg_dl);
    bool any_images = false;
    for (const auto *const image : imageDiskList) {
        if (image) {
            any_images = true;
            break;
        }
    }

    // unconditionally enable the interrupt flag
    CALLBACK_SIF(true);

    /* map out functions 0x40-0x48 if not emulating INT 13h extensions */
    if (!int13_extensions_enable && reg_ah >= 0x40 && reg_ah <= 0x48) {
        LOG(LOG_BIOS,LOG_WARN)("INT 13h: Guest requested disabled extensions (AH=0x%02x); set 'int 13 extensions=1' to enable them",
                               reg_ah);
        reg_ah=0xff;
        CALLBACK_SCF(true);
        return CBRET_NONE;
    }

    // NOTE: the 0xff error code returned in some cases is questionable; 0x01 seems more correct
    switch(reg_ah) {
    case 0x0: /* Reset disk */
        {
            /* if there aren't any diskimages (so only localdrives and virtual drives)
             * always succeed on reset disk. If there are diskimages then and only then
             * do real checks
             */
            if (any_images && driveInactive(drivenum)) {
                /* driveInactive sets carry flag if the specified drive is not available */
                if ((machine==MCH_CGA) || (machine==MCH_AMSTRAD) || (machine==MCH_PCJR) || (machine==MCH_OLIVETTI) || (machine==MCH_3270PC)) {
                    /* those bioses call floppy drive reset for invalid drive values */
                    if (((imageDiskList[0]) && (imageDiskList[0]->active)) || ((imageDiskList[1]) && (imageDiskList[1]->active))) {
                        if (machine!=MCH_PCJR && reg_dl<0x80) reg_ip++;
                        last_status = Int13Status::NoError;
                        CALLBACK_SCF(false);
                    }
                }
                return CBRET_NONE;
            }
            if (machine!=MCH_PCJR && reg_dl<0x80) reg_ip++;
            if (reg_dl >= 0x80) IDE_ResetDiskByBIOS(reg_dl);
            last_status = Int13Status::NoError;
            CALLBACK_SCF(false);
        }
        break;
    case 0x1: /* Get status of last operation */

        if(last_status != Int13Status::NoError) {
            reg_ah = (uint8_t)last_status;
            CALLBACK_SCF(true);
        } else {
            reg_ah = 0x00;
            CALLBACK_SCF(false);
        }
        break;
    case 0x2: /* Read sectors */
        return INT13_ReadSectors(drivenum);
    case 0x3: /* Write sectors */
        
        if(driveInactive(drivenum) || !imageDiskList[drivenum]) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        /* INT 13h is limited to 512 bytes/sector (as far as I know).
         * The sector buffer in this function is limited to 512 bytes/sector,
         * so this is also a protection against overrunning the stack if you
         * mount a PC-98 disk image (1024 bytes/sector) and try to read it with INT 13h. */
        if (imageDiskList[drivenum]->sector_size > sizeof(sectbuf)) {
            LOG(LOG_BIOS,LOG_WARN)("INT 13h: Write failed because drive 0x%02x has %u-byte sectors",
                                   reg_dl,
                                   imageDiskList[drivenum]->sector_size);

            imageDiskChange[drivenum] = false;

            reg_ah = 0x80; /* timeout */
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        /* See INT 13h AH=02h read: floppies use 8-bit cylinder/8-bit sector, hard disks 10-bit/6-bit. */
        rd_cyl  = (reg_dl & 0x80) ? (uint32_t)(reg_ch | ((reg_cl & 0xc0) << 2)) : (uint32_t)reg_ch;
        rd_sect = (reg_dl & 0x80) ? (uint32_t)(reg_cl & 63) : (uint32_t)reg_cl;
        bufptr = reg_bx;
        /* Floppies transfer the bytes/sector given by the INT 1Eh disk base table; hard disks use the image sector size. */
        secsz = (reg_dl & 0x80) ? imageDiskList[drivenum]->getSectSize() : floppyDPT_sector_size();
        if (secsz > sizeof(sectbuf)) {
            LOG(LOG_BIOS,LOG_WARN)("INT 13h: DPT bytes/sector %u exceeds buffer, clamping to %u",
                                   static_cast<unsigned int>(secsz),
                                   static_cast<unsigned int>(sizeof(sectbuf)));
            secsz = sizeof(sectbuf);
        }
        for(i=0;i<reg_al;i++) {
            for(t=0;t<secsz;t++) {
                sectbuf[t] = real_readb(SegValue(es),bufptr);
                bufptr++;
            }

            if(drivenum < 2)
                diskio_delay(512, 0); // Floppy
            else
                diskio_delay(512);

            last_status = imageDiskList[drivenum]->Write_Sector((uint32_t)reg_dh, rd_cyl, rd_sect + i, &sectbuf[0]);
            if(last_status != Int13Status::NoError) {
                reg_ah = (uint8_t)last_status;
                CALLBACK_SCF(true);
                return CBRET_NONE;
            }
        }
        reg_ah = 0x00;
        CALLBACK_SCF(false);
        break;
    case 0x04: /* Verify sectors */
        if (reg_al==0) {
            reg_ah = 0x01;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        if(driveInactive(drivenum)) {
            reg_ah = (uint8_t)last_status;
            return CBRET_NONE;
        }

        /* TODO: Finish coding this section */
        /*
        segat = SegValue(es);
        bufptr = reg_bx;
        for(i=0;i<reg_al;i++) {
            last_status = imageDiskList[drivenum]->Read_Sector((uint32_t)reg_dh, rd_cyl, rd_sect+i, sectbuf);
            if(last_status != 0x00) {
                LOG(LOG_BIOS,LOG_DEBUG)("INT 13h: Verify failed on drive 0x%02x", reg_dl);
                CALLBACK_SCF(true);
                return CBRET_NONE;
            }
            for(t=0;t<512;t++) {
                real_writeb(segat,bufptr,sectbuf[t]);
                bufptr++;
            }
        }*/
        reg_ah = 0x00;
        //Qbix: The following codes don't match my specs. al should be number of sector verified
        //reg_al = 0x10; /* CRC verify failed */
        //reg_al = 0x00; /* CRC verify succeeded */
        CALLBACK_SCF(false);
          
        break;
    case 0x05: /* Format track */
        /* ignore it. I just fucking want FORMAT.COM to write the FAT structure for God's sake */
        LOG(LOG_BIOS,LOG_WARN)("INT 13h: Format track is ignored");
        if (driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        CALLBACK_SCF(false);
        reg_ah = 0x00;
        break;
    case 0x06: /* Format track set bad sector flags */
        /* ignore it. I just fucking want FORMAT.COM to write the FAT structure for God's sake */
        LOG(LOG_BIOS,LOG_WARN)("INT 13h: Format track bad-sector flags are ignored (AH=06h)");
        if (driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        CALLBACK_SCF(false);
        reg_ah = 0x00;
        break;
    case 0x07: /* Format track set bad sector flags */
        /* ignore it. I just fucking want FORMAT.COM to write the FAT structure for God's sake */
        LOG(LOG_BIOS,LOG_WARN)("INT 13h: Format track bad-sector flags are ignored (AH=07h)");
        if (driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        CALLBACK_SCF(false);
        reg_ah = 0x00;
        break;
    case 0x08: /* Get drive parameters */
        if(driveInactive(drivenum)) {
            if(drivenum == 0) {
                // if no floppy drive 0 mounted, return fixed values for 1.44MB drive
                // This fixes a problem in some DOS applications that occurred when booting from the hard disk without a mounted floppy disk.
                reg_ax = 0x00;
                reg_bl = 4;
                reg_ch = 79;
                reg_cl = 18;
                reg_dh = 1;
                reg_dl = 1;
                last_status = Int13Status::NoError;
                CALLBACK_SCF(false);
                {/*fill in ES:DI*/
                    /* Even though BIOSes document vectors pointed at tables,
                     * return the address in BIOS because Windows 95 apparently
                     * likes to replace INT 0x1E with a pointer to whatever else
                     * it decides, and then complain that the floppy controller
                     * isn't working without ever touching I/O ports. What a jackass. */
                    reg_di = floppyparm0 & 0xFu;
                    CPU_SetSegGeneral(es,floppyparm0 >> 4u);
                }
                return CBRET_NONE;
            }
            last_status = Int13Status::DriveParamFailed;
            reg_ah = (uint8_t)last_status;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        reg_ax = 0x00;
        reg_bl = imageDiskList[drivenum]->GetBiosType();
        uint32_t tmpheads, tmpcyl, tmpsect, tmpsize;
        imageDiskList[drivenum]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
        if (tmpcyl==0) LOG(LOG_BIOS,LOG_ERROR)("INT 13h: Drive 0x%02x has zero cylinders", reg_dl);
        else tmpcyl--;      // cylinder count -> max cylinder
        if (tmpheads==0) LOG(LOG_BIOS,LOG_ERROR)("INT 13h: Drive 0x%02x has zero heads", reg_dl);
        else tmpheads--;    // head count -> max head

        /* older BIOSes were known to subtract 1 or 2 additional "reserved" cylinders.
         * some code, such as Windows 3.1 WDCTRL, might assume that fact. emulate that here */
        {
            uint32_t reserv = imageDiskList[drivenum]->Get_Reserved_Cylinders();
            if (tmpcyl > reserv) tmpcyl -= reserv;
            else tmpcyl = 0;
        }

        reg_ch = (uint8_t)(tmpcyl & 0xff);
        if (reg_dl & 0x80) /* hard disk: 10-bit cylinder, 6-bit sector */
            reg_cl = (uint8_t)(((tmpcyl >> 2) & 0xc0) | (tmpsect & 0x3f));
        else /* floppy: 8-bit cylinder in CH, 8-bit sector in CL */
            reg_cl = (uint8_t)(tmpsect & 0xff);
        reg_dh = (uint8_t)tmpheads;
        last_status = Int13Status::NoError;
        if (reg_dl&0x80) {  // harddisks
            /* do NOT fill in ES:DI for hard disks, it causes Windows 95 to crash at startup! */
            reg_dl = 0;
            for (int index = 2; index < MAX_DISK_IMAGES; index++) {
                if (imageDiskList[index] != NULL) reg_dl++;
            }
        } else {        // floppy disks
            {/*fill in ES:DI*/
                /* Even though BIOSes document vectors pointed at tables,
                 * return the address in BIOS because Windows 95 apparently
                 * likes to replace INT 0x1E with a pointer to whatever else
                 * it decides, and then complain that the floppy controller
                 * isn't working without ever touching I/O ports. What a jackass. */
                if (reg_dl < 2) {
                    reg_di = (floppyparm0 & 0xFu) + (reg_dl * 11u);
                    CPU_SetSegGeneral(es,floppyparm0 >> 4u);
                }
                else {
                    reg_di = 0;
                    CPU_SetSegGeneral(es,0);
                }
            }
            reg_dl = 0;
            if(imageDiskList[0] != NULL) reg_dl++;
            if(imageDiskList[1] != NULL) reg_dl++;
        }
        CALLBACK_SCF(false);
        break;
    case 0x11: /* Recalibrate drive */
        reg_ah = 0x00;
        CALLBACK_SCF(false);
        break;
    case 0x15: /* Get disk type */
        /* Korean Powerdolls uses this to detect harddrives */
        LOG(LOG_BIOS,LOG_DEBUG)("INT 13h: Get disk type requested for drive 0x%02x", reg_dl);
        if (any_images) {
            if(driveInactive(drivenum)) {
                last_status = Int13Status::DriveParamFailed;
                reg_ah = (uint8_t)last_status;
                CALLBACK_SCF(true);
                return CBRET_NONE;
            }
            imageDiskList[drivenum]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
            uint64_t largesize = tmpheads*tmpcyl*tmpsect*tmpsize;
            largesize/=512;
            uint32_t ts = static_cast<uint32_t>(largesize);
            reg_ah = (drivenum < 2)?(int13_disk_change_detect_enable?2:1):3; //With 2 for floppy MSDOS starts calling int 13 ah 16
            if(reg_ah == 3) {
                reg_cx = static_cast<uint16_t>(ts >>16);
                reg_dx = static_cast<uint16_t>(ts & 0xffff);
            }
            CALLBACK_SCF(false);
        } else {
            if (drivenum <DOS_DRIVES && (Drives[drivenum] || drivenum <2)) {
                if (drivenum <2) {
                    //TODO use actual size (using 1.44 for now).
                    reg_ah = (int13_disk_change_detect_enable?2:1); // type
//                  reg_cx = 0;
//                  reg_dx = 2880; //Only set size for harddrives.
                } else {
                    //TODO use actual size (using 105 mb for now).
                    reg_ah = 0x3; // type
                    reg_cx = 3;
                    reg_dx = 0x4800;
                }
                CALLBACK_SCF(false);
            } else { 
                LOG(LOG_BIOS,LOG_DEBUG)("INT 13h: Get disk type requested for unavailable drive 0x%02x", reg_dl);
                reg_ah=0xff;
                CALLBACK_SCF(true);
            }
        }
        break;
    case 0x16: /* Detect disk change (apparently added to XT BIOSes in 1986 according to RBIL) */
	if (int13_disk_change_detect_enable) {
		LOG(LOG_BIOS,LOG_DEBUG)("INT 13h: Detect disk change requested for drive 0x%02x", reg_dl);
		if(driveInactive(drivenum)) {
			last_status = Int13Status::Timeout;
			reg_ah = (uint8_t)last_status;
			CALLBACK_SCF(true);
		}
		else if (drivenum < 2) {
			if (imageDiskChange[drivenum]) {
				imageDiskChange[drivenum] = false;
				last_status = Int13Status::DiskChanged; // change line active
				reg_ah = (uint8_t)last_status;
				CALLBACK_SCF(true);
			}
			else {
				last_status = Int13Status::NoError; // no change
				reg_ah = (uint8_t)last_status;
				CALLBACK_SCF(false);
			}
		}
		else {
			last_status = Int13Status::DiskChanged; // not supported (because it's a hard drive)
			reg_ah = (uint8_t)last_status;
			CALLBACK_SCF(true);
		}
	}
	else {
		reg_ah=0xff; // not supported
		CALLBACK_SCF(true);
	}
	break;
    case 0x17: /* Set disk type for format */
        /* Pirates! needs this to load */
        killRead = true;
        reg_ah = 0x00;
        CALLBACK_SCF(false);
        break;
    case 0x41: /* Check Extensions Present */
        if ((reg_bx == 0x55aa) && !(driveInactive(drivenum))) {
            LOG(LOG_BIOS,LOG_DEBUG)("INT 13h: Extensions requested for drive 0x%02x", reg_dl);
            reg_ah=0x1; /* 1.x extension supported */
            reg_bx=0xaa55;  /* Extensions installed */
            reg_cx=0x1; /* Extended disk access functions (AH=42h-44h,47h,48h) supported */
            CALLBACK_SCF(false);
            break;
        }
        LOG(LOG_BIOS,LOG_DEBUG)("INT 13h: Extensions are unavailable for drive 0x%02x (BX=0x%04x)", reg_dl, reg_bx);
        CALLBACK_SCF(true);
        break;
    case 0x42: /* Extended Read Sectors From Drive */
        return INT13_ExtendedReadSectors(drivenum, any_images);
    case 0x43: { /* Extended Write Sectors to Drive */
        if(driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        DiskAddressPacket packet = {};
        if (!ReadDiskAddressPacket(SegValue(ds), reg_si, packet) ||
            packet.sector_count == 0) {
            reg_ah = 0x01;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        bufptr = packet.buffer_offset;
        for(i=0;i<packet.sector_count;i++) {
            for(t=0;t<imageDiskList[drivenum]->getSectSize();t++) {
                sectbuf[t] = real_readb(packet.buffer_segment,bufptr);
                bufptr++;
            }

            if(drivenum < 2)
                diskio_delay(512, 0); // Floppy
            else
                diskio_delay(512);

            last_status = imageDiskList[drivenum]->Write_AbsoluteSector(packet.sector+i, &sectbuf[0]);
            if(last_status != Int13Status::NoError) {
                reg_ah = (uint8_t)last_status;
                CALLBACK_SCF(true);
                return CBRET_NONE;
            }
        }
        reg_ah = 0x00;
        CALLBACK_SCF(false);
        break;
    }
    case 0x44: /* Extended Verify Sectors [http://www.ctyme.com/intr/rb-0711.htm] */
        if(driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        /* Just signal success, we don't actually verify anything */
        reg_ah = 0x00;
        CALLBACK_SCF(false);
        break;
    case 0x48: { /* get drive parameters */
        uint16_t bufsz;

        if(driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        segat = SegValue(ds);
        bufptr = reg_si;
        bufsz = real_readw(segat,bufptr+0);
        if (bufsz < 0x1A) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        if (bufsz > 0x1E) bufsz = 0x1E;
        else bufsz = 0x1A;

        tmpheads = tmpcyl = tmpsect = tmpsize = 0;
        if(!IDE_GetPhysGeometry(drivenum, tmpheads, tmpcyl, tmpsect, tmpsize, LBA)) {
            imageDiskList[drivenum]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
            LBA = imageDiskList[drivenum]->getLBA();
        }
        real_writew(segat,bufptr+0x00,bufsz);
        real_writew(segat,bufptr+0x02,0x0003);  /* C/H/S valid, DMA boundary errors handled */
        real_writed(segat,bufptr+0x04,tmpcyl);
        real_writed(segat,bufptr+0x08,tmpheads);
        real_writed(segat,bufptr+0x0C,tmpsect);
        real_writed(segat,bufptr+0x10, int13_enable_48bitLBA?(uint32_t)(LBA & 0xFFFFFFFF): (uint32_t)(LBA > 0x0FFFFFFF?0x0FFFFFFF:LBA)); /* LBA lower 32bit */
        real_writed(segat,bufptr+0x14, int13_enable_48bitLBA?(uint32_t)(LBA >> 32):0); /* LBA upper 32bit */
        real_writew(segat,bufptr+0x18,512);
        if (bufsz >= 0x1E)
            real_writed(segat,bufptr+0x1A,0xFFFFFFFF); /* no EDD information available */

        reg_ah = 0x00;
        CALLBACK_SCF(false);
        } break;
    case 0x4B: /* Terminate disk emulation or get emulation status */
        /* NTS: Windows XP CD-ROM boot requires this call to work or else setup cannot find its own files. */
        if (reg_dl == 0x7F || (INT13_ElTorito_NoEmuDriveNumber != 0 && INT13_ElTorito_NoEmuDriveNumber == reg_dl)) {
            if (reg_al <= 1) {
                PhysPt p = (SegValue(ds) << 4) + reg_si;
                phys_writeb(p + 0x00,0x13);
                phys_writeb(p + 0x01,(0/*no emulation*/) + ((INT13_ElTorito_IDEInterface >= 0) ? 0x40 : 0));
                phys_writeb(p + 0x02,INT13_ElTorito_NoEmuDriveNumber);
                if (INT13_ElTorito_IDEInterface >= 0) {
                        phys_writeb(p + 0x03,(unsigned char)(INT13_ElTorito_IDEInterface >> 1)); /* which IDE controller */
                        phys_writew(p + 0x08,INT13_ElTorito_IDEInterface & 1);/* bit 0: IDE master/slave */
                }
                else {
                        phys_writeb(p + 0x03,0);
                        phys_writew(p + 0x08,0);
                }
                phys_writed(p + 0x04,0);
                phys_writew(p + 0x0A,0);
                phys_writew(p + 0x0C,0);
                phys_writew(p + 0x0E,0);
                phys_writeb(p + 0x10,0);
                phys_writeb(p + 0x11,0);
                phys_writeb(p + 0x12,0);
                reg_ah = 0x00;
                CALLBACK_SCF(false);
                break;
            }
            else {
                reg_ah=0xff;
                CALLBACK_SCF(true);
                return CBRET_NONE;
            }
        }
        else {
            reg_ah=0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        break;
    default:
        LOG(LOG_BIOS,LOG_WARN)("INT 13h: Unsupported function AH=0x%02x requested for drive 0x%02x", reg_ah, reg_dl);
        reg_ah=0xff;
        CALLBACK_SCF(true);
    }
    return CBRET_NONE;
}

void CALLBACK_DeAllocate(Bitu in);

Bitu BIOS_GetINT13Callback()
{
    return call_int13;
}

void BIOS_UnsetupINT13()
{
    if (call_int13 != 0) {
        CALLBACK_DeAllocate(call_int13);
        RealSetVec(0x13,0); /* zero INT 13h for now */
        call_int13 = 0;
    }
}

void BIOS_SetupINT13()
{
    call_int13 = CALLBACK_Allocate();
    CALLBACK_Setup(call_int13, &INT13_DiskHandler, CB_INT13, "Int 13 Bios disk");
    RealSetVec(0x13, CALLBACK_RealPointer(call_int13));
    killRead = false;
}
