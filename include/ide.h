/*
 * IDE ATA/ATAPI emulation
 */

#ifndef DOSBOX_IDE_H
#define DOSBOX_IDE_H

void IDE_Auto(signed char &index,bool &slave);
void IDE_CDROM_Attach(signed char index,bool slave,unsigned char drive_index);
void IDE_Hard_Disk_Attach(signed char index,bool slave,unsigned char bios_drive_index);
void IDE_ResetDiskByBIOS(unsigned char disk);

#endif
