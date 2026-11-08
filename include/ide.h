/*
 * IDE ATA/ATAPI emulation
 */

#ifndef DOSBOX_IDE_H
#define DOSBOX_IDE_H

#include <stdint.h>
#include "setup.h"

#define MAX_IDE_CONTROLLERS 	8

extern const char *ide_names[MAX_IDE_CONTROLLERS];
extern void (*ide_inits[MAX_IDE_CONTROLLERS])(Section *);

void IDE_Auto(signed char &index,bool &slave);
void IDE_CDROM_Attach(signed char index,bool slave,unsigned char drive_index);
void IDE_CDROM_Detach(unsigned char drive_index);
bool IDE_CDROM_Detach(signed char index,bool slave);
bool IDE_CDROM_Detach(const std::string &opts);
bool IDE_CDROM_Eject(int index,bool slave);
void IDE_CDROM_Detach_Ret(signed char &indexret,bool &slaveret,unsigned char drive_index);
void IDE_Hard_Disk_Attach(signed char index,bool slave,unsigned char bios_disk_index);
void IDE_Hard_Disk_Detach(unsigned char bios_disk_index);
void IDE_ResetDiskByBIOS(unsigned char disk);
bool IDE_controller_occupied(signed char index, bool slave);
void DOS_EnableDriveIDEMenu(unsigned int idx,unsigned char ms);
bool IDE_is_CDROM(signed char index,bool slave);
bool IDE_is_CDROM(const std::string &opts);

/* BIOS hard disk access (INT 13h, PC-98 INT 1Bh) to a disk that is attached to IDE emulation: the access
 * is carried out by the emulated drive's mechanics as the BIOS would carry it out with ATA commands (spin
 * up if the drive was put in standby, seek, wait for the sectors to rotate under the heads, read-ahead
 * buffer, write cache), so that BIOS and direct IDE access see the same drive. */
enum class IDEBiosDiskOp {
	Read,
	Write,
	Verify,
	Seek,
	Recalibrate
};

/* disk_index: index into imageDiskList[] (2 = first hard disk). True if the disk is attached to IDE. */
bool IDE_BIOS_DiskAttached(unsigned int disk_index);

/* Account for the access and wait (with interrupts enabled) as long as it takes, unless the drive is in
 * instant mode. Returns false (and does nothing) if the disk is not attached to IDE. */
bool IDE_BIOS_DiskAccess(unsigned int disk_index,uint64_t lba,unsigned int count,IDEBiosDiskOp op);

#endif
