/*
 * IDE ATA/ATAPI emulation
 */

#ifndef DOSBOX_IDE_H
#define DOSBOX_IDE_H

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

#endif
